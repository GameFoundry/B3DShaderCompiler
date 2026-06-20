/*
 * OpaqueStructResolver.cpp
 */

#include "OpaqueStructResolver.h"
#include "AST.h"
#include "ASTFactory.h"
#include "Exception.h"
#include "ReportIdents.h"
#include "Helper.h"
#include "TypeDenoter.h"
#include "Converter.h"

#include <algorithm>


namespace Xsc
{


void OpaqueStructResolver::Resolve(Program& program, const NameMangling& nameMangling)
{
    nameMangling_ = nameMangling;

    RewriteFunctionSignatures(program);
    RewriteFunctionBodies(program);
    StripOpaqueMembersFromStructs(program);
}


/* ----- Helpers ----- */

StructDecl* OpaqueStructResolver::ResolveOpaqueStruct(const TypeDenoterPtr& typeDen)
{
    if (!typeDen)
        return nullptr;
    const TypeDenoter* t = &typeDen->GetAliased();
    if (auto structTypeDen = t->As<StructTypeDenoter>())
    {
        if (auto sd = structTypeDen->structDeclRef)
        {
            if (sd->HasOpaqueMember())
                return sd;
        }
    }
    return nullptr;
}

void OpaqueStructResolver::CollectOpaqueFields(StructDecl* structDecl, std::vector<std::pair<std::string, TypeDenoterPtr>>& outFields)
{
    if (!structDecl)
        return;

    /* Include base struct fields first (matches member layout order). */
    if (structDecl->baseStructRef)
        CollectOpaqueFields(structDecl->baseStructRef, outFields);

    for (const auto& member : structDecl->varMembers)
    {
        auto typeDen = member->typeSpecifier->GetTypeDenoter();
        if (Converter::IsOpaqueTypeDenoter(typeDen))
        {
            for (const auto& varDecl : member->varDecls)
                outFields.emplace_back(varDecl->ident, typeDen);
        }
    }
}


/* ----- Pass 1: rewrite function signatures ----- */

void OpaqueStructResolver::SplitOpaqueParameter(FunctionDecl& funcDecl, std::size_t actualIndex, std::size_t logicalIndex, FunctionRewriteInfo& info)
{
    auto& param = funcDecl.parameters[actualIndex];
    auto structDecl = ResolveOpaqueStruct(param->typeSpecifier->typeDenoter);
    if (!structDecl)
        return;

    /* Collect the opaque fields of the struct in declaration order. */
    std::vector<std::pair<std::string, TypeDenoterPtr>> opaqueFields;
    CollectOpaqueFields(structDecl, opaqueFields);
    if (opaqueFields.empty())
        return;

    /* Generate one new parameter VarDeclStmnt per opaque field. */
    std::vector<VarDecl*> newParamVarDecls;
    std::vector<std::string> newFieldNames;
    newParamVarDecls.reserve(opaqueFields.size());
    newFieldNames.reserve(opaqueFields.size());

    /* Pick a base identifier from the original parameter's first VarDecl (parameters
       always have exactly one VarDecl in HLSL grammar). */
    const std::string baseIdent = (!param->varDecls.empty() ? param->varDecls.front()->ident : std::string("p"));
    VarDecl* originalParamVarDecl = (!param->varDecls.empty() ? param->varDecls.front().get() : nullptr);

    /* The synthesized parameters will be inserted right after the original parameter. */
    std::vector<VarDeclStmntPtr> newParamStmnts;
    newParamStmnts.reserve(opaqueFields.size());

    for (const auto& field : opaqueFields)
    {
        /* Build a fresh type denoter for the new parameter from the opaque field's type. */
        auto typeSpec = ASTFactory::MakeTypeSpecifier(field.second->Copy());
        auto stmnt = ASTFactory::MakeVarDeclStmnt(typeSpec, baseIdent + "_" + field.first);
        stmnt->flags << VarDeclStmnt::isParameter;
        if (!stmnt->varDecls.empty())
        {
            auto newVarDecl = stmnt->varDecls.front().get();
            newParamVarDecls.push_back(newVarDecl);
            newFieldNames.push_back(field.first);
        }
        newParamStmnts.push_back(stmnt);
    }

    /* Insert the synthesized parameter statements immediately after the original. */
    funcDecl.parameters.insert(
        funcDecl.parameters.begin() + actualIndex + 1,
        newParamStmnts.begin(), newParamStmnts.end()
    );

    /* Record the rewrite info under the LOGICAL (original) parameter slot. */
    if (info.opaqueParamsPerOriginal.size() <= logicalIndex)
    {
        info.opaqueParamsPerOriginal.resize(logicalIndex + 1);
        info.opaqueFieldsPerOriginal.resize(logicalIndex + 1);
    }
    info.opaqueParamsPerOriginal[logicalIndex] = std::move(newParamVarDecls);
    info.opaqueFieldsPerOriginal[logicalIndex] = std::move(newFieldNames);

    /* (Alias maps for these parameters are seeded later in VisitFunctionDecl when we
       enter the function body, so that they are live only inside that body.) */
    (void)originalParamVarDecl;
}

/* Walks the program and finds all FunctionDecls (both global and member functions). */
static void ForEachFunctionDecl(Program& program, const std::function<void(FunctionDecl&)>& fn)
{
    for (const auto& stmnt : program.globalStmnts)
    {
        if (auto basic = stmnt->As<BasicDeclStmnt>())
        {
            if (auto funcDecl = basic->declObject->As<FunctionDecl>())
            {
                fn(*funcDecl);
                continue;
            }
            if (auto structDecl = basic->declObject->As<StructDecl>())
            {
                for (auto& member : structDecl->funcMembers)
                {
                    if (member)
                        fn(*member);
                }
            }
        }
    }
}

void OpaqueStructResolver::RewriteFunctionSignatures(Program& program)
{
    ForEachFunctionDecl(program, [this](FunctionDecl& fd)
    {
        FunctionRewriteInfo info;
        info.originalParamCount = fd.parameters.size();
        info.opaqueParamsPerOriginal.resize(info.originalParamCount);
        info.opaqueFieldsPerOriginal.resize(info.originalParamCount);

        bool anyRewrite = false;
        /* Iterate over the ORIGINAL slot positions only; do not visit the synthesized
           parameters that get appended (they cannot be opaque-bearing structs). The
           i-th original parameter is at index i + sum(opaqueParamsPerOriginal[j].size())
           for j < i, since SplitOpaqueParameter inserts right after the original. */
        for (std::size_t i = 0; i < info.originalParamCount; ++i)
        {
            std::size_t actualIndex = i;
            for (std::size_t j = 0; j < i; ++j)
                actualIndex += info.opaqueParamsPerOriginal[j].size();

            if (actualIndex >= fd.parameters.size())
                break;

            auto& param = fd.parameters[actualIndex];
            if (ResolveOpaqueStruct(param->typeSpecifier->typeDenoter))
            {
                SplitOpaqueParameter(fd, actualIndex, i, info);
                anyRewrite = true;
            }
        }

        if (anyRewrite)
            funcRewrites_[&fd] = std::move(info);
    });
}


/* ----- Pass 2: walk function bodies ----- */

void OpaqueStructResolver::RewriteFunctionBodies(Program& program)
{
    Visit(&program);
}

OpaqueStructResolver::AliasMap* OpaqueStructResolver::FindAliasMap(VarDecl* localVar)
{
    auto it = activeAliasMaps_.find(localVar);
    return (it == activeAliasMaps_.end() ? nullptr : &it->second);
}

const OpaqueStructResolver::AliasMap* OpaqueStructResolver::FindAliasMap(VarDecl* localVar) const
{
    auto it = activeAliasMaps_.find(localVar);
    return (it == activeAliasMaps_.end() ? nullptr : &it->second);
}

Decl* OpaqueStructResolver::ResolveOpaqueFieldAccess(VarDecl* localVar, const std::string& fieldName, const AST* errorContext)
{
    auto* m = FindAliasMap(localVar);
    if (!m)
        return nullptr;
    auto it = m->find(fieldName);
    if (it == m->end())
    {
        RuntimeErr(R_OpaqueStructUninitialized(fieldName), errorContext);
        return nullptr;
    }
    if (it->second.ambiguous)
    {
        RuntimeErr(R_OpaqueStructAmbiguousAlias(fieldName), errorContext);
        return nullptr;
    }
    if (it->second.target == nullptr)
    {
        RuntimeErr(R_OpaqueStructUninitialized(fieldName), errorContext);
        return nullptr;
    }
    return it->second.target;
}

/* Extracts the global Decl referenced by an expression that denotes an opaque-typed
   global (e.g. `g_tex`, or a parameter VarDecl of opaque type). The decl can be a
   BufferDecl (Texture/Buffer) or SamplerDecl (sampler-state) for module-level
   declarations, or a VarDecl for opaque function parameters. Returns null if the
   expression does not directly reference such a Decl. */
static Decl* ResolveOpaqueExprToDecl(Expr* expr)
{
    if (!expr)
        return nullptr;

    if (auto bracket = expr->As<BracketExpr>())
        return ResolveOpaqueExprToDecl(bracket->expr.get());

    if (auto objExpr = expr->As<ObjectExpr>())
    {
        if (objExpr->symbolRef)
        {
            switch (objExpr->symbolRef->Type())
            {
                case AST::Types::VarDecl:
                case AST::Types::BufferDecl:
                case AST::Types::SamplerDecl:
                    return objExpr->symbolRef;
                default:
                    break;
            }
        }
    }
    return nullptr;
}

void OpaqueStructResolver::InitAliasFromInitializer(VarDecl* localVar, StructDecl* structDecl, Expr* initializer)
{
    AliasMap m;
    std::vector<std::pair<std::string, TypeDenoterPtr>> opaqueFields;
    CollectOpaqueFields(structDecl, opaqueFields);

    /* Seed with all opaque fields unset (target == nullptr). */
    for (const auto& f : opaqueFields)
        m[f.first] = AliasEntry{};

    if (!initializer)
    {
        activeAliasMaps_[localVar] = std::move(m);
        return;
    }

    /* Case A: copy-init from another opaque-bearing struct variable. The initializer
       references another local VarDecl whose type is the same opaque-bearing struct. */
    if (auto rhsDecl = ResolveOpaqueExprToDecl(initializer))
    {
        if (auto rhsVar = rhsDecl->As<VarDecl>())
        {
            if (auto* rhsMap = FindAliasMap(rhsVar))
            {
                m = *rhsMap;
                activeAliasMaps_[localVar] = std::move(m);
                return;
            }
        }
    }

    /* Case B: aggregate-initializer { expr0, expr1, ... } matching struct layout. */
    if (auto initExpr = initializer->As<InitializerExpr>())
    {
        /* Walk struct members in declaration order; for opaque members, peel one
           initializer expression and resolve to a global VarDecl. */
        std::size_t initIdx = 0;
        const auto& exprs = initExpr->exprs;

        std::function<void(StructDecl*)> walk = [&](StructDecl* sd)
        {
            if (!sd) return;
            if (sd->baseStructRef)
                walk(sd->baseStructRef);
            for (const auto& member : sd->varMembers)
            {
                auto memberType = member->typeSpecifier->GetTypeDenoter();
                const bool isOpaque = Converter::IsOpaqueTypeDenoter(memberType);
                for (const auto& vd : member->varDecls)
                {
                    if (initIdx >= exprs.size())
                        return;
                    if (isOpaque)
                    {
                        if (auto target = ResolveOpaqueExprToDecl(exprs[initIdx].get()))
                        {
                            AliasEntry e;
                            e.target = target;
                            m[vd->ident] = e;
                        }
                    }
                    ++initIdx;
                }
            }
        };
        walk(structDecl);
    }

    activeAliasMaps_[localVar] = std::move(m);
}

OpaqueStructResolver::AliasMap OpaqueStructResolver::JoinAliasMaps(const AliasMap& a, const AliasMap& b)
{
    AliasMap r = a;
    for (auto& kv : r)
    {
        auto it = b.find(kv.first);
        if (it == b.end())
        {
            kv.second.ambiguous = true;
            kv.second.target = nullptr;
        }
        else
        {
            if (kv.second.target != it->second.target ||
                kv.second.ambiguous || it->second.ambiguous)
            {
                kv.second.ambiguous = true;
                kv.second.target = nullptr;
            }
        }
    }
    return r;
}


#define IMPLEMENT_VISIT_PROC(AST_NAME) \
    void OpaqueStructResolver::Visit##AST_NAME(AST_NAME* ast, void* args)


IMPLEMENT_VISIT_PROC(FunctionDecl)
{
    /* Seed alias maps for opaque-bearing struct parameters of this function from the
       signature rewriting that already happened. */
    auto it = funcRewrites_.find(ast);
    if (it != funcRewrites_.end())
    {
        const auto& info = it->second;
        std::size_t actualIndex = 0;
        for (std::size_t i = 0; i < info.originalParamCount; ++i)
        {
            if (actualIndex >= ast->parameters.size())
                break;
            auto& origParam = ast->parameters[actualIndex];
            if (!origParam->varDecls.empty() && !info.opaqueParamsPerOriginal[i].empty())
            {
                VarDecl* localVar = origParam->varDecls.front().get();
                AliasMap m;
                for (std::size_t k = 0; k < info.opaqueParamsPerOriginal[i].size(); ++k)
                {
                    AliasEntry e;
                    e.target = info.opaqueParamsPerOriginal[i][k];
                    m[info.opaqueFieldsPerOriginal[i][k]] = e;
                }
                activeAliasMaps_[localVar] = std::move(m);
            }
            /* Advance past this original + any opaque params it spawned. */
            actualIndex += 1 + info.opaqueParamsPerOriginal[i].size();
        }
    }

    PushFunctionDecl(ast);
    Visit(ast->codeBlock);
    PopFunctionDecl();

    /* Clear alias maps that belong to this function's parameters. */
    if (it != funcRewrites_.end())
    {
        for (auto& origParam : ast->parameters)
        {
            if (!origParam->varDecls.empty())
                activeAliasMaps_.erase(origParam->varDecls.front().get());
        }
    }
}

IMPLEMENT_VISIT_PROC(CodeBlock)
{
    Visit(ast->stmnts);
}

IMPLEMENT_VISIT_PROC(VarDeclStmnt)
{
    /* Visit type specifier and initializers first (so any opaque accesses inside are rewritten). */
    Visit(ast->typeSpecifier);
    for (auto& vd : ast->varDecls)
    {
        if (vd->initializer)
            Visit(vd->initializer);
    }

    /* For each VarDecl in this statement of opaque-bearing struct type (parameters
       handled in VisitFunctionDecl above), build an alias map from its initializer. */
    if (auto structDecl = ResolveOpaqueStruct(ast->typeSpecifier->typeDenoter))
    {
        if (!ast->flags(VarDeclStmnt::isParameter) && !InsideStructDecl())
        {
            for (auto& vd : ast->varDecls)
            {
                InitAliasFromInitializer(vd.get(), structDecl, vd->initializer.get());

                /* Strip opaque initializer entries: rebuild the initializer to contain
                   only POD-member entries (preserving order). */
                if (vd->initializer)
                {
                    if (auto initExpr = vd->initializer->As<InitializerExpr>())
                    {
                        std::vector<ExprPtr> kept;
                        std::size_t initIdx = 0;
                        const auto& exprs = initExpr->exprs;
                        std::function<void(StructDecl*)> walk = [&](StructDecl* sd)
                        {
                            if (!sd) return;
                            if (sd->baseStructRef)
                                walk(sd->baseStructRef);
                            for (const auto& member : sd->varMembers)
                            {
                                auto memberType = member->typeSpecifier->GetTypeDenoter();
                                const bool isOpaque = Converter::IsOpaqueTypeDenoter(memberType);
                                for (std::size_t k = 0; k < member->varDecls.size(); ++k)
                                {
                                    if (initIdx >= exprs.size())
                                        return;
                                    if (!isOpaque)
                                        kept.push_back(exprs[initIdx]);
                                    ++initIdx;
                                }
                            }
                        };
                        walk(structDecl);
                        initExpr->exprs = std::move(kept);

                        /* If the struct is going to be empty after stripping (the
                           generator will add a dummy int), and the initializer ended
                           up empty too, drop the initializer entirely so we don't emit
                           an invalid `{}` initializer. */
                        if (initExpr->exprs.empty())
                            vd->initializer.reset();
                    }
                    else
                    {
                        /* If the initializer was a copy-init from another
                           opaque-bearing struct var (Case A) and the struct becomes
                           empty after stripping, drop the initializer. */
                        if (structDecl->NumMemberVariables() == 0 ||
                            std::all_of(
                                structDecl->varMembers.begin(),
                                structDecl->varMembers.end(),
                                [](const VarDeclStmntPtr& m){
                                    return Converter::IsOpaqueTypeDenoter(
                                        m->typeSpecifier->GetTypeDenoter());
                                }))
                        {
                            vd->initializer.reset();
                        }
                    }
                }
            }
        }
    }
}

IMPLEMENT_VISIT_PROC(ExprStmnt)
{
    /* Detect a straight-line opaque-field assignment: `localVar.opaqueField = rhs;`.
       Update the alias map and mark the entire statement as dead so the GLSL emit
       pass skips it. */
    if (auto ax = ast->expr ? ast->expr->As<AssignExpr>() : nullptr)
    {
        if (auto lhsObj = ax->lvalueExpr ? ax->lvalueExpr->As<ObjectExpr>() : nullptr)
        {
            if (lhsObj->prefixExpr)
            {
                if (auto prefixObj = lhsObj->prefixExpr->As<ObjectExpr>())
                {
                    if (auto localVar = prefixObj->FetchVarDecl())
                    {
                        if (auto* m = FindAliasMap(localVar))
                        {
                            if (m->find(lhsObj->ident) != m->end())
                            {
                                if (auto target = ResolveOpaqueExprToDecl(ax->rvalueExpr.get()))
                                {
                                    AliasEntry e;
                                    e.target = target;
                                    (*m)[lhsObj->ident] = e;
                                }
                                else
                                {
                                    AliasEntry e;
                                    e.ambiguous = true;
                                    (*m)[lhsObj->ident] = e;
                                }
                                /* Replace the assignment's operands with NullExpr so
                                   downstream passes (ExprConverter) don't traverse into
                                   ObjectExprs that reference soon-to-be-stripped struct
                                   members. Mark the stmnt dead so GLSLConverter drops it. */
                                ax->lvalueExpr = std::make_shared<NullExpr>(SourcePosition::ignore);
                                ax->rvalueExpr = std::make_shared<NullExpr>(SourcePosition::ignore);
                                ast->flags << AST::isDeadCode;
                                return;
                            }
                        }
                    }
                }
            }
        }
    }

    Visit(ast->attribs);
    Visit(ast->expr);
}

IMPLEMENT_VISIT_PROC(AssignExpr)
{
    /* AssignExpr not directly inside an ExprStmnt: just recurse into children so any
       contained ObjectExpr opaque-field reads get rewritten. We intentionally do not
       attempt to rewrite assignments here; the supported form is the straight-line
       ExprStmnt case handled above. */
    Visit(ast->lvalueExpr);
    Visit(ast->rvalueExpr);
}

IMPLEMENT_VISIT_PROC(ObjectExpr)
{
    /* Recursively visit the prefix first (so nested rewrites happen). */
    if (ast->prefixExpr)
        Visit(ast->prefixExpr);

    /* Detect `localVar.opaqueField` access pattern and rewrite in-place to a direct
       reference to the resolved global Decl. Only opaque fields are in the alias map,
       so POD member access (e.g. `tc.tint`) falls through unchanged. */
    if (ast->prefixExpr)
    {
        if (auto prefixObj = ast->prefixExpr->As<ObjectExpr>())
        {
            if (auto localVar = prefixObj->FetchVarDecl())
            {
                if (auto* m = FindAliasMap(localVar))
                {
                    if (m->find(ast->ident) != m->end())
                    {
                        if (auto target = ResolveOpaqueFieldAccess(localVar, ast->ident, ast))
                        {
                            ast->ReplaceSymbol(target);
                            ast->prefixExpr.reset();
                        }
                    }
                }
            }
        }
    }
}

IMPLEMENT_VISIT_PROC(CallExpr)
{
    /* Visit prefix and arguments first to rewrite any contained ObjectExprs. */
    if (ast->prefixExpr)
        Visit(ast->prefixExpr);
    for (auto& a : ast->arguments)
        Visit(a);

    /* If the callee has a rewrite info, decompose opaque-bearing struct arguments. */
    auto funcDecl = ast->GetFunctionImpl();
    if (!funcDecl)
        funcDecl = ast->GetFunctionDecl();
    if (!funcDecl)
        return;

    auto it = funcRewrites_.find(funcDecl);
    if (it == funcRewrites_.end())
        return;

    const auto& info = it->second;

    /* Build the new argument list. For each original-parameter slot, keep the original
       argument expr, then append one resolved-opaque arg per opaque field. */
    std::vector<ExprPtr> newArgs;
    newArgs.reserve(ast->arguments.size());

    if (ast->arguments.size() < info.originalParamCount)
    {
        /* Fewer args than original params: default arguments fill the rest; the
           reference analyzer / function call resolver attaches defaultArgumentRefs. We
           can only safely decompose explicit arguments. */
    }

    std::size_t argIdx = 0;
    for (std::size_t i = 0; i < info.originalParamCount; ++i)
    {
        if (argIdx >= ast->arguments.size())
            break;
        ExprPtr origArg = ast->arguments[argIdx++];
        newArgs.push_back(origArg);

        if (!info.opaqueParamsPerOriginal[i].empty())
        {
            /* Resolve each opaque field of the arg via the alias map of the local
               opaque-bearing struct variable passed as the argument. */
            VarDecl* localVar = nullptr;
            if (auto d = ResolveOpaqueExprToDecl(origArg.get()))
                localVar = d->As<VarDecl>();
            const auto* m = (localVar ? FindAliasMap(localVar) : nullptr);
            for (std::size_t k = 0; k < info.opaqueFieldsPerOriginal[i].size(); ++k)
            {
                const auto& fieldName = info.opaqueFieldsPerOriginal[i][k];
                Decl* target = nullptr;
                if (m)
                {
                    auto mit = m->find(fieldName);
                    if (mit != m->end() && !mit->second.ambiguous && mit->second.target)
                        target = mit->second.target;
                }
                if (!target)
                {
                    RuntimeErr(R_OpaqueStructAmbiguousAlias(fieldName), ast);
                    return;
                }
                newArgs.push_back(ASTFactory::MakeObjectExpr(target));
            }
        }
    }
    ast->arguments = std::move(newArgs);
}

IMPLEMENT_VISIT_PROC(IfStmnt)
{
    if (ast->condition)
        Visit(ast->condition);

    /* Snapshot alias state. */
    auto before = activeAliasMaps_;
    Visit(ast->bodyStmnt);
    auto afterIf = activeAliasMaps_;
    activeAliasMaps_ = before;

    if (ast->elseStmnt)
        Visit(ast->elseStmnt);

    /* Join: any field that diverged becomes ambiguous. */
    for (auto& kv : activeAliasMaps_)
    {
        auto itIf = afterIf.find(kv.first);
        if (itIf != afterIf.end())
            kv.second = JoinAliasMaps(kv.second, itIf->second);
    }
    /* Also propagate maps that may have been newly introduced only in if branch. */
    for (const auto& kv : afterIf)
    {
        if (activeAliasMaps_.find(kv.first) == activeAliasMaps_.end())
            activeAliasMaps_[kv.first] = kv.second;
    }
}

IMPLEMENT_VISIT_PROC(ElseStmnt)
{
    Visit(ast->bodyStmnt);
}

/* Loops are conservative: any opaque field reassigned in the body becomes ambiguous on
   exit, since iteration count is unknown. We implement this by visiting once with a
   snapshot and joining body-end with body-start. */
IMPLEMENT_VISIT_PROC(ForLoopStmnt)
{
    if (ast->initStmnt)
        Visit(ast->initStmnt);
    if (ast->condition)
        Visit(ast->condition);
    if (ast->iteration)
        Visit(ast->iteration);

    auto before = activeAliasMaps_;
    Visit(ast->bodyStmnt);
    for (auto& kv : activeAliasMaps_)
    {
        auto itBefore = before.find(kv.first);
        if (itBefore != before.end())
            kv.second = JoinAliasMaps(kv.second, itBefore->second);
    }
}

IMPLEMENT_VISIT_PROC(WhileLoopStmnt)
{
    if (ast->condition)
        Visit(ast->condition);
    auto before = activeAliasMaps_;
    Visit(ast->bodyStmnt);
    for (auto& kv : activeAliasMaps_)
    {
        auto itBefore = before.find(kv.first);
        if (itBefore != before.end())
            kv.second = JoinAliasMaps(kv.second, itBefore->second);
    }
}

IMPLEMENT_VISIT_PROC(DoWhileLoopStmnt)
{
    auto before = activeAliasMaps_;
    Visit(ast->bodyStmnt);
    if (ast->condition)
        Visit(ast->condition);
    for (auto& kv : activeAliasMaps_)
    {
        auto itBefore = before.find(kv.first);
        if (itBefore != before.end())
            kv.second = JoinAliasMaps(kv.second, itBefore->second);
    }
}

IMPLEMENT_VISIT_PROC(SwitchStmnt)
{
    if (ast->selector)
        Visit(ast->selector);
    auto before = activeAliasMaps_;
    AliasMap joinAccum;
    bool first = true;
    std::unordered_map<VarDecl*, AliasMap> accum;
    for (auto& c : ast->cases)
    {
        activeAliasMaps_ = before;
        Visit(c);
        if (first)
        {
            accum = activeAliasMaps_;
            first = false;
        }
        else
        {
            for (auto& kv : accum)
            {
                auto it2 = activeAliasMaps_.find(kv.first);
                if (it2 != activeAliasMaps_.end())
                    kv.second = JoinAliasMaps(kv.second, it2->second);
            }
        }
    }
    if (!first)
        activeAliasMaps_ = accum;
}


#undef IMPLEMENT_VISIT_PROC


/* ----- Pass 3: strip opaque members from struct decls ----- */

void OpaqueStructResolver::StripOpaqueMembersFromStructs(Program& program)
{
    std::function<void(StructDecl*)> stripOne = [&](StructDecl* sd)
    {
        if (!sd || !sd->HasOpaqueMember())
            return;

        for (auto it = sd->varMembers.begin(); it != sd->varMembers.end(); )
        {
            auto& member = *it;
            auto typeDen = member->typeSpecifier->GetTypeDenoter();
            if (Converter::IsOpaqueTypeDenoter(typeDen))
            {
                it = sd->varMembers.erase(it);
                continue;
            }
            ++it;
        }

        /* Also strip from localStmnts so the generator's local-stmnt walk does not
           re-emit them. */
        for (auto it = sd->localStmnts.begin(); it != sd->localStmnts.end(); )
        {
            auto* s = it->get();
            if (auto vds = s->As<VarDeclStmnt>())
            {
                auto typeDen = vds->typeSpecifier->GetTypeDenoter();
                if (Converter::IsOpaqueTypeDenoter(typeDen))
                {
                    it = sd->localStmnts.erase(it);
                    continue;
                }
            }
            ++it;
        }
    };

    for (auto& stmnt : program.globalStmnts)
    {
        if (auto basic = stmnt->As<BasicDeclStmnt>())
        {
            if (auto sd = basic->declObject->As<StructDecl>())
                stripOne(sd);
        }
    }
}


} // /namespace Xsc



// ================================================================================
