/*
 * ASTCloner.cpp
 *
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include "ASTCloner.h"
#include "AST.h"


namespace Xsc
{


TypeDenoterPtr ASTCloner::CloneTypeDenoter(const TypeDenoterPtr& typeDenoter)
{
    if (!typeDenoter)
        return nullptr;

    if (auto arrayType = typeDenoter->As<ArrayTypeDenoter>())
    {
        return std::make_shared<ArrayTypeDenoter>(
            CloneTypeDenoter(arrayType->subTypeDenoter),
            Clone(arrayType->arrayDims)
        );
    }

    if (auto bufferType = typeDenoter->As<BufferTypeDenoter>())
    {
        auto clone = std::make_shared<BufferTypeDenoter>(*bufferType);
        clone->genericTypeDenoter = CloneTypeDenoter(bufferType->genericTypeDenoter);
        clone->bufferDeclRef = nullptr;
        return clone;
    }

    if (auto samplerType = typeDenoter->As<SamplerTypeDenoter>())
    {
        auto clone = std::make_shared<SamplerTypeDenoter>(*samplerType);
        clone->samplerDeclRef = nullptr;
        return clone;
    }

    if (auto structType = typeDenoter->As<StructTypeDenoter>())
    {
        auto clone = std::make_shared<StructTypeDenoter>(*structType);
        clone->structDeclRef = nullptr;
        clone->templateArguments.clear();
        for (const auto& argument : structType->templateArguments)
            clone->templateArguments.push_back(CloneTypeDenoter(argument));
        return clone;
    }

    if (auto aliasType = typeDenoter->As<AliasTypeDenoter>())
    {
        auto clone = std::make_shared<AliasTypeDenoter>(*aliasType);
        clone->aliasDeclRef = nullptr;
        return clone;
    }

    if (auto functionType = typeDenoter->As<FunctionTypeDenoter>())
    {
        auto clone = std::make_shared<FunctionTypeDenoter>(*functionType);
        clone->funcDeclRefs.clear();
        return clone;
    }

    return typeDenoter->Copy();
}

void ASTCloner::CloneStatementBase(const Stmnt* source, Stmnt* clone)
{
    clone->attribs = Clone(source->attribs);
}


/*
 * ======= Visitor implementation: =======
 */

#define IMPLEMENT_VISIT_PROC(AST_NAME) \
    void ASTCloner::Visit##AST_NAME(AST_NAME* ast, void* args)

IMPLEMENT_VISIT_PROC(Program)
{
    auto clone = CloneLeaf(ast);
    clone->globalStmnts = Clone(ast->globalStmnts);
    clone->disabledAST = Clone(ast->disabledAST);
    clone->entryPointRef = nullptr;
    clone->usedIntrinsics.clear();
    clone->usedMatrixSubscripts.clear();
    clone->layoutTessControl.patchConstFunctionRef = nullptr;
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(CodeBlock)
{
    auto clone = CloneLeaf(ast);
    clone->stmnts = Clone(ast->stmnts);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(Attribute)
{
    auto clone = CloneLeaf(ast);
    clone->arguments = Clone(ast->arguments);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(SwitchCase)
{
    auto clone = CloneLeaf(ast);
    clone->expr = Clone(ast->expr);
    clone->stmnts = Clone(ast->stmnts);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(SamplerValue)
{
    auto clone = CloneLeaf(ast);
    clone->value = Clone(ast->value);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(Register)
{
    clonedAST_ = CloneLeaf(ast);
}

IMPLEMENT_VISIT_PROC(PackOffset)
{
    clonedAST_ = CloneLeaf(ast);
}

IMPLEMENT_VISIT_PROC(ArrayDimension)
{
    auto clone = CloneTypedLeaf(ast);
    clone->expr = Clone(ast->expr);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(TypeSpecifier)
{
    auto clone = CloneTypedLeaf(ast);
    clone->structDecl = Clone(ast->structDecl);
    clone->typeDenoter = CloneTypeDenoter(ast->typeDenoter);
    clonedAST_ = clone;
}

/* --- Declarations --- */

IMPLEMENT_VISIT_PROC(VarDecl)
{
    auto clone = CloneTypedLeaf(ast);
    clone->namespaceExpr = Clone(ast->namespaceExpr);
    clone->arrayDims = Clone(ast->arrayDims);
    clone->packOffset = Clone(ast->packOffset);
    clone->annotations = Clone(ast->annotations);
    clone->initializer = Clone(ast->initializer);
    clone->customTypeDenoter = CloneTypeDenoter(ast->customTypeDenoter);
    clone->declStmntRef = nullptr;
    clone->bufferDeclRef = nullptr;
    clone->structDeclRef = nullptr;
    clone->staticMemberVarRef = nullptr;
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(BufferDecl)
{
    auto clone = CloneTypedLeaf(ast);
    clone->arrayDims = Clone(ast->arrayDims);
    clone->slotRegisters = Clone(ast->slotRegisters);
    clone->annotations = Clone(ast->annotations);
    clone->initializer = Clone(ast->initializer);
    clone->declStmntRef = nullptr;
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(SamplerDecl)
{
    auto clone = CloneTypedLeaf(ast);
    clone->arrayDims = Clone(ast->arrayDims);
    clone->slotRegisters = Clone(ast->slotRegisters);
    clone->samplerValues = Clone(ast->samplerValues);
    clone->declStmntRef = nullptr;
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(StructDecl)
{
    auto clone = CloneTypedLeaf(ast);
    clone->templateArguments.clear();
    for (const auto& type : ast->templateArguments)
        clone->templateArguments.push_back(CloneTypeDenoter(type));
    clone->localStmnts = Clone(ast->localStmnts);
    clone->varMembers.clear();
    clone->funcMembers.clear();
    clone->declStmntRef = nullptr;
    clone->baseStructRef = nullptr;
    clone->compatibleStructRef = nullptr;
    clone->systemValuesRef.clear();
    clone->parentStructDeclRefs.clear();
    clone->shaderOutputVarDeclRefs.clear();

    for (const auto& statement : clone->localStmnts)
    {
        if (auto varDeclStmnt = std::dynamic_pointer_cast<VarDeclStmnt>(statement))
        {
            clone->varMembers.push_back(varDeclStmnt);
            for (const auto& varDecl : varDeclStmnt->varDecls)
                varDecl->structDeclRef = clone.get();
        }
        else if (auto basicDeclStmnt = std::dynamic_pointer_cast<BasicDeclStmnt>(statement))
        {
            if (auto functionDecl = std::dynamic_pointer_cast<FunctionDecl>(basicDeclStmnt->declObject))
            {
                clone->funcMembers.push_back(functionDecl);
                functionDecl->structDeclRef = clone.get();
            }
        }
    }

    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(AliasDecl)
{
    auto clone = CloneTypedLeaf(ast);
    clone->typeDenoter = CloneTypeDenoter(ast->typeDenoter);
    clone->declStmntRef = nullptr;
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(FunctionDecl)
{
    auto clone = CloneTypedLeaf(ast);
    clone->returnType = Clone(ast->returnType);
    clone->parameters = Clone(ast->parameters);
    clone->annotations = Clone(ast->annotations);
    clone->codeBlock = Clone(ast->codeBlock);
    clone->inputSemantics = {};
    clone->outputSemantics = {};
    clone->declStmntRef = nullptr;
    clone->funcImplRef = nullptr;
    clone->funcForwardDeclRefs.clear();
    clone->structDeclRef = nullptr;
    clone->paramStructs.clear();
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(UniformBufferDecl)
{
    auto clone = CloneTypedLeaf(ast);
    clone->slotRegisters = Clone(ast->slotRegisters);
    clone->localStmnts = Clone(ast->localStmnts);
    clone->varMembers.clear();
    clone->declStmntRef = nullptr;

    for (const auto& statement : clone->localStmnts)
    {
        if (auto varDeclStmnt = std::dynamic_pointer_cast<VarDeclStmnt>(statement))
        {
            clone->varMembers.push_back(varDeclStmnt);
            for (const auto& varDecl : varDeclStmnt->varDecls)
                varDecl->bufferDeclRef = clone.get();
        }
    }

    clonedAST_ = clone;
}

/* --- Declaration statements --- */

IMPLEMENT_VISIT_PROC(BufferDeclStmnt)
{
    auto clone = CloneLeaf(ast);
    CloneStatementBase(ast, clone.get());
    clone->typeDenoter = CloneTypeDenoterAs(ast->typeDenoter);
    clone->bufferDecls = Clone(ast->bufferDecls);
    for (const auto& bufferDecl : clone->bufferDecls)
        bufferDecl->declStmntRef = clone.get();
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(SamplerDeclStmnt)
{
    auto clone = CloneLeaf(ast);
    CloneStatementBase(ast, clone.get());
    clone->typeDenoter = CloneTypeDenoterAs(ast->typeDenoter);
    clone->samplerDecls = Clone(ast->samplerDecls);
    for (const auto& samplerDecl : clone->samplerDecls)
        samplerDecl->declStmntRef = clone.get();
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(VarDeclStmnt)
{
    auto clone = CloneLeaf(ast);
    CloneStatementBase(ast, clone.get());
    clone->typeSpecifier = Clone(ast->typeSpecifier);
    clone->varDecls = Clone(ast->varDecls);
    for (const auto& varDecl : clone->varDecls)
        varDecl->declStmntRef = clone.get();
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(AliasDeclStmnt)
{
    auto clone = CloneLeaf(ast);
    CloneStatementBase(ast, clone.get());
    clone->structDecl = Clone(ast->structDecl);
    clone->aliasDecls = Clone(ast->aliasDecls);
    for (const auto& aliasDecl : clone->aliasDecls)
        aliasDecl->declStmntRef = clone.get();
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(BasicDeclStmnt)
{
    auto clone = CloneLeaf(ast);
    CloneStatementBase(ast, clone.get());
    clone->declObject = Clone(ast->declObject);

    if (auto structDecl = std::dynamic_pointer_cast<StructDecl>(clone->declObject))
        structDecl->declStmntRef = clone.get();
    else if (auto functionDecl = std::dynamic_pointer_cast<FunctionDecl>(clone->declObject))
        functionDecl->declStmntRef = clone.get();
    else if (auto uniformBufferDecl = std::dynamic_pointer_cast<UniformBufferDecl>(clone->declObject))
        uniformBufferDecl->declStmntRef = clone.get();

    clonedAST_ = clone;
}

/* --- Statements --- */

IMPLEMENT_VISIT_PROC(NullStmnt)
{
    auto clone = CloneLeaf(ast);
    CloneStatementBase(ast, clone.get());
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(CodeBlockStmnt)
{
    auto clone = CloneLeaf(ast);
    CloneStatementBase(ast, clone.get());
    clone->codeBlock = Clone(ast->codeBlock);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(ForLoopStmnt)
{
    auto clone = CloneLeaf(ast);
    CloneStatementBase(ast, clone.get());
    clone->initStmnt = Clone(ast->initStmnt);
    clone->condition = Clone(ast->condition);
    clone->iteration = Clone(ast->iteration);
    clone->bodyStmnt = Clone(ast->bodyStmnt);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(WhileLoopStmnt)
{
    auto clone = CloneLeaf(ast);
    CloneStatementBase(ast, clone.get());
    clone->condition = Clone(ast->condition);
    clone->bodyStmnt = Clone(ast->bodyStmnt);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(DoWhileLoopStmnt)
{
    auto clone = CloneLeaf(ast);
    CloneStatementBase(ast, clone.get());
    clone->bodyStmnt = Clone(ast->bodyStmnt);
    clone->condition = Clone(ast->condition);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(IfStmnt)
{
    auto clone = CloneLeaf(ast);
    CloneStatementBase(ast, clone.get());
    clone->condition = Clone(ast->condition);
    clone->bodyStmnt = Clone(ast->bodyStmnt);
    clone->elseStmnt = Clone(ast->elseStmnt);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(ElseStmnt)
{
    auto clone = CloneLeaf(ast);
    CloneStatementBase(ast, clone.get());
    clone->bodyStmnt = Clone(ast->bodyStmnt);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(SwitchStmnt)
{
    auto clone = CloneLeaf(ast);
    CloneStatementBase(ast, clone.get());
    clone->selector = Clone(ast->selector);
    clone->cases = Clone(ast->cases);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(ExprStmnt)
{
    auto clone = CloneLeaf(ast);
    CloneStatementBase(ast, clone.get());
    clone->expr = Clone(ast->expr);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(ReturnStmnt)
{
    auto clone = CloneLeaf(ast);
    CloneStatementBase(ast, clone.get());
    clone->expr = Clone(ast->expr);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(CtrlTransferStmnt)
{
    auto clone = CloneLeaf(ast);
    CloneStatementBase(ast, clone.get());
    clonedAST_ = clone;
}

/* --- Expressions --- */

IMPLEMENT_VISIT_PROC(NullExpr)
{
    clonedAST_ = CloneTypedLeaf(ast);
}

IMPLEMENT_VISIT_PROC(SequenceExpr)
{
    auto clone = CloneTypedLeaf(ast);
    clone->exprs = Clone(ast->exprs);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(LiteralExpr)
{
    clonedAST_ = CloneTypedLeaf(ast);
}

IMPLEMENT_VISIT_PROC(TypeSpecifierExpr)
{
    auto clone = CloneTypedLeaf(ast);
    clone->typeSpecifier = Clone(ast->typeSpecifier);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(TernaryExpr)
{
    auto clone = CloneTypedLeaf(ast);
    clone->condExpr = Clone(ast->condExpr);
    clone->thenExpr = Clone(ast->thenExpr);
    clone->elseExpr = Clone(ast->elseExpr);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(BinaryExpr)
{
    auto clone = CloneTypedLeaf(ast);
    clone->lhsExpr = Clone(ast->lhsExpr);
    clone->rhsExpr = Clone(ast->rhsExpr);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(UnaryExpr)
{
    auto clone = CloneTypedLeaf(ast);
    clone->expr = Clone(ast->expr);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(PostUnaryExpr)
{
    auto clone = CloneTypedLeaf(ast);
    clone->expr = Clone(ast->expr);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(CallExpr)
{
    auto clone = CloneTypedLeaf(ast);
    clone->prefixExpr = Clone(ast->prefixExpr);
    clone->typeDenoter = CloneTypeDenoter(ast->typeDenoter);
    clone->explicitTemplateArgs.clear();
    for (const auto& type : ast->explicitTemplateArgs)
        clone->explicitTemplateArgs.push_back(CloneTypeDenoter(type));
    clone->arguments = Clone(ast->arguments);
    clone->funcDeclRef = nullptr;
    clone->intrinsic = Intrinsic::Undefined;
    clone->defaultArgumentRefs.clear();
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(BracketExpr)
{
    auto clone = CloneTypedLeaf(ast);
    clone->expr = Clone(ast->expr);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(AssignExpr)
{
    auto clone = CloneTypedLeaf(ast);
    clone->lvalueExpr = Clone(ast->lvalueExpr);
    clone->rvalueExpr = Clone(ast->rvalueExpr);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(ObjectExpr)
{
    auto clone = CloneTypedLeaf(ast);
    clone->prefixExpr = Clone(ast->prefixExpr);
    clone->symbolRef = nullptr;
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(ArrayExpr)
{
    auto clone = CloneTypedLeaf(ast);
    clone->prefixExpr = Clone(ast->prefixExpr);
    clone->arrayIndices = Clone(ast->arrayIndices);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(CastExpr)
{
    auto clone = CloneTypedLeaf(ast);
    clone->typeSpecifier = Clone(ast->typeSpecifier);
    clone->expr = Clone(ast->expr);
    clonedAST_ = clone;
}

IMPLEMENT_VISIT_PROC(InitializerExpr)
{
    auto clone = CloneTypedLeaf(ast);
    clone->exprs = Clone(ast->exprs);
    clonedAST_ = clone;
}

#undef IMPLEMENT_VISIT_PROC


} // /namespace Xsc
