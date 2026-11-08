/*
 * InlineRayTracingFieldAnalyzer.cpp
 *
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include "InlineRayTracingFieldAnalyzer.h"
#include "AST.h"
#include <stdexcept>


namespace Xsc
{


InlineRayTracingFieldAnalyzer::InlineRayTracingFieldAnalyzer(Program& program) :
    program_ { program }
{
}

InlineRayTracingHLSL::FieldUsage InlineRayTracingFieldAnalyzer::AnalyzeHitFields()
{
    Reset("RayHit");
    followCalls_ = false;

    if (structDecl_)
        Visit(&program_);

    return usage_;
}

InlineRayTracingHLSL::FieldUsage InlineRayTracingFieldAnalyzer::AnalyzeCandidateFields(FunctionDecl* funcDecl)
{
    Reset("RayCandidate");
    followCalls_ = true;

    if (structDecl_)
    {
        pendingFuncDecls_.push_back(funcDecl);
        while (!pendingFuncDecls_.empty())
        {
            auto pendingFuncDecl = pendingFuncDecls_.back();
            pendingFuncDecls_.pop_back();

            if (visitedFuncDecls_.insert(pendingFuncDecl).second)
                Visit(pendingFuncDecl);
        }
    }

    return usage_;
}


/*
 * ======= Private: =======
 */

#define IMPLEMENT_VISIT_PROC(AST_NAME) \
    void InlineRayTracingFieldAnalyzer::Visit##AST_NAME(AST_NAME* ast, void* args)

IMPLEMENT_VISIT_PROC(FunctionDecl)
{
    if (!ast->codeBlock || (!followCalls_ && !ast->flags(AST::isReachable)))
        return;

    /* Member functions of structures declared in a function have their own scope */
    auto prevFuncDecl       = currentFuncDecl_;
    auto prevIsEntryPoint   = isEntryPoint_;
    auto prevLocalVarDecls  = std::move(localVarDecls_);

    currentFuncDecl_    = ast;
    isEntryPoint_       =
    (
        ast->flags(FunctionDecl::isEntryPoint) ||
        ast->flags(FunctionDecl::isSecondaryEntryPoint) ||
        ast->rayTracingStage != ShaderTarget::Undefined
    );
    localVarDecls_.clear();

    /* Entry point parameters are shader inputs and outputs, not local storage */
    if (!isEntryPoint_)
    {
        for (const auto& param : ast->parameters)
        {
            for (const auto& varDecl : param->varDecls)
                localVarDecls_.insert(varDecl.get());
        }
    }

    Visit(ast->codeBlock);

    currentFuncDecl_    = prevFuncDecl;
    isEntryPoint_       = prevIsEntryPoint;
    localVarDecls_      = std::move(prevLocalVarDecls);
}

IMPLEMENT_VISIT_PROC(VarDecl)
{
    if (currentFuncDecl_)
    {
        localVarDecls_.insert(ast);
        AllowWholeValue(ast->initializer.get());
    }

    VISIT_DEFAULT(VarDecl);
}

IMPLEMENT_VISIT_PROC(ExprStmnt)
{
    AllowWholeValue(ast->expr.get());
    VISIT_DEFAULT(ExprStmnt);
}

IMPLEMENT_VISIT_PROC(ReturnStmnt)
{
    if (!isEntryPoint_)
        AllowWholeValue(ast->expr.get());

    VISIT_DEFAULT(ReturnStmnt);
}

IMPLEMENT_VISIT_PROC(SequenceExpr)
{
    Check(ast);
    VISIT_DEFAULT(SequenceExpr);
}

IMPLEMENT_VISIT_PROC(TernaryExpr)
{
    Check(ast);
    AllowWholeValue(ast->thenExpr.get());
    AllowWholeValue(ast->elseExpr.get());
    VISIT_DEFAULT(TernaryExpr);
}

IMPLEMENT_VISIT_PROC(CallExpr)
{
    Check(ast);

    if (ast->intrinsic == Intrinsic::Undefined)
    {
        if (auto funcDecl = ast->GetFunctionDecl())
        {
            /* Arguments of output parameters are written back, so they must be local storage */
            for (std::size_t i = 0; i < ast->arguments.size() && i < funcDecl->parameters.size(); ++i)
            {
                auto argExpr = ast->arguments[i].get();
                if (!funcDecl->parameters[i]->IsOutput() || IsLocalStorage(argExpr))
                    AllowWholeValue(argExpr);
            }

            if (followCalls_)
            {
                if (auto funcImpl = ast->GetFunctionImpl())
                    pendingFuncDecls_.push_back(funcImpl);
            }
        }
    }

    VISIT_DEFAULT(CallExpr);
}

IMPLEMENT_VISIT_PROC(BracketExpr)
{
    Check(ast);
    AllowWholeValue(ast->expr.get());
    VISIT_DEFAULT(BracketExpr);
}

IMPLEMENT_VISIT_PROC(AssignExpr)
{
    Check(ast);

    if (ast->op == AssignOp::Set && IsLocalStorage(ast->lvalueExpr.get()))
    {
        AllowWholeValue(ast->lvalueExpr.get());
        AllowWholeValue(ast->rvalueExpr.get());
    }

    VISIT_DEFAULT(AssignExpr);
}

IMPLEMENT_VISIT_PROC(ObjectExpr)
{
    Check(ast);

    if (ast->prefixExpr && !ast->isStatic && IsAnalyzedType(ast->prefixExpr.get(), false))
    {
        usage_.fields.insert(ast->ident);
        AllowWholeValue(ast->prefixExpr.get());
    }

    VISIT_DEFAULT(ObjectExpr);
}

IMPLEMENT_VISIT_PROC(ArrayExpr)
{
    Check(ast);

    if (IsAnalyzedType(ast->prefixExpr.get(), true))
        AllowWholeValue(ast->prefixExpr.get());

    VISIT_DEFAULT(ArrayExpr);
}

IMPLEMENT_VISIT_PROC(CastExpr)
{
    Check(ast);
    VISIT_DEFAULT(CastExpr);
}

IMPLEMENT_VISIT_PROC(InitializerExpr)
{
    Check(ast);
    VISIT_DEFAULT(InitializerExpr);
}

#undef IMPLEMENT_VISIT_PROC

void InlineRayTracingFieldAnalyzer::Reset(const std::string& structIdent)
{
    structDecl_ = nullptr;
    for (const auto& stmnt : program_.builtInStmnts)
    {
        if (auto basicDeclStmnt = stmnt->As<BasicDeclStmnt>())
        {
            if (auto structDecl = basicDeclStmnt->declObject->As<StructDecl>())
            {
                if (structDecl->ident == structIdent && structDecl->flags(AST::isRayTracingBuiltin))
                    structDecl_ = structDecl;
            }
        }
    }

    /* Without the structure there's nothing to analyze, so every field counts as read */
    usage_              = InlineRayTracingHLSL::FieldUsage();
    usage_.allFields    = (structDecl_ == nullptr);

    wholeValueExprs_.clear();
    pendingFuncDecls_.clear();
    visitedFuncDecls_.clear();
    currentFuncDecl_    = nullptr;
    isEntryPoint_       = false;
    localVarDecls_.clear();
}

bool InlineRayTracingFieldAnalyzer::IsAnalyzedType(Expr* expr, bool allowArray) const
{
    if (!structDecl_ || !expr)
        return false;

    try
    {
        const TypeDenoter* type = &(expr->GetTypeDenoter()->GetAliased());

        if (allowArray)
        {
            if (auto arrayType = type->As<ArrayTypeDenoter>())
                type = (arrayType->subTypeDenoter ? &(arrayType->subTypeDenoter->GetAliased()) : nullptr);
        }

        if (type)
        {
            if (auto structType = type->As<StructTypeDenoter>())
                return (structType->structDeclRef == structDecl_);
        }
    }
    catch (const std::exception&)
    {
        /* Expressions without a type (e.g. namespaces) aren't values of the structure */
    }

    return false;
}

bool InlineRayTracingFieldAnalyzer::IsLocalStorage(Expr* expr) const
{
    if (!expr)
        return false;

    if (auto bracketExpr = expr->As<BracketExpr>())
        return IsLocalStorage(bracketExpr->expr.get());

    if (auto arrayExpr = expr->As<ArrayExpr>())
        return IsLocalStorage(arrayExpr->prefixExpr.get());

    if (auto objectExpr = expr->As<ObjectExpr>())
        return (!objectExpr->prefixExpr && localVarDecls_.count(objectExpr->FetchVarDecl()) != 0);

    return false;
}

void InlineRayTracingFieldAnalyzer::AllowWholeValue(const Expr* expr)
{
    if (expr)
        wholeValueExprs_.insert(expr);
}

void InlineRayTracingFieldAnalyzer::Check(Expr* expr)
{
    if (wholeValueExprs_.count(expr) == 0 && IsAnalyzedType(expr, true))
        usage_.allFields = true;
}


} // /namespace Xsc



// ================================================================================
