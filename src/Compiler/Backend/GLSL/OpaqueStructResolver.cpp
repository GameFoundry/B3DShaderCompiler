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

    funcRewrites_.clear();
    activeAliasMaps_.clear();
    funcSummaries_.clear();
    callReturnAliases_.clear();
    processedFuncs_.clear();

    RewriteFunctionSignatures(program);
    RewriteFunctionBodies(program);
    StripOpaqueMembersFromStructs(program);
}


/* ----- Helpers ----- */

StructDecl* OpaqueStructResolver::TryGetOpaqueStructDeclaration(const TypeDenoterPtr& typeDen)
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

void OpaqueStructResolver::CollectOpaqueFields(StructDecl* structDecl, std::vector<std::pair<std::string, TypeDenoterPtr>>& outFields, const std::string& prefix)
{
    if (!structDecl)
        return;

    /* Include base struct fields first (matches member layout order). */
    if (structDecl->baseStructRef)
        CollectOpaqueFields(structDecl->baseStructRef, outFields, prefix);

    for (const auto& member : structDecl->varMembers)
    {
        auto typeDen = member->typeSpecifier->GetTypeDenoter();
        if (Converter::IsOpaqueTypeDenoter(typeDen))
        {
            for (const auto& varDecl : member->varDecls)
                outFields.emplace_back(prefix + varDecl->ident, typeDen);
        }
        else if (auto nestedStruct = TryGetOpaqueStructDeclaration(typeDen))
        {
            /* Recurse into a nested opaque-bearing struct member, extending the dotted
               access path (e.g. "mat.albedo"). Pure-POD struct members resolve to null
               here and are left untouched. */
            for (const auto& varDecl : member->varDecls)
                CollectOpaqueFields(nestedStruct, outFields, prefix + varDecl->ident + ".");
        }
    }
}

bool OpaqueStructResolver::StructIsFullyOpaque(StructDecl* structDecl)
{
    if (!structDecl)
        return true;

    if (structDecl->baseStructRef && !StructIsFullyOpaque(structDecl->baseStructRef))
        return false;

    for (const auto& member : structDecl->varMembers)
    {
        auto typeDen = member->typeSpecifier->GetTypeDenoter();
        if (Converter::IsOpaqueTypeDenoter(typeDen))
            continue;                                   // opaque leaf: removed by stripping
        if (auto nested = TryGetOpaqueStructDeclaration(typeDen))
        {
            if (!StructIsFullyOpaque(nested))
                return false;
            continue;
        }
        return false;                                   // a POD (kept) member exists
    }
    return true;
}

void OpaqueStructResolver::BuildInitializerSlots(StructDecl* structDecl, std::size_t exprCount, std::vector<InitSlot>& outSlots)
{
    std::size_t idx = 0;
    std::function<void(StructDecl*, const std::string&)> walk =
        [&](StructDecl* sd, const std::string& prefix)
    {
        if (!sd)
            return;
        if (sd->baseStructRef)
            walk(sd->baseStructRef, prefix);
        for (const auto& member : sd->varMembers)
        {
            auto memberType = member->typeSpecifier->GetTypeDenoter();
            const bool isOpaque = Converter::IsOpaqueTypeDenoter(memberType);
            StructDecl* nested = (isOpaque ? nullptr : TryGetOpaqueStructDeclaration(memberType));
            for (const auto& vd : member->varDecls)
            {
                if (idx >= exprCount)
                    return;
                if (isOpaque)
                {
                    InitSlot s;
                    s.kind      = InitSlot::Kind::Opaque;
                    s.path      = prefix + vd->ident;
                    s.exprIndex = idx;
                    outSlots.push_back(std::move(s));
                    ++idx;
                }
                else if (nested)
                {
                    if (StructIsFullyOpaque(nested))
                    {
                        /* The nested struct collapses to one dummy int in the residual,
                           but its opaque leaves still need their aliases seeded; record
                           them with the initializer slots they consume. */
                        InitSlot s;
                        s.kind = InitSlot::Kind::EmptyNestedDummy;
                        std::vector<std::pair<std::string, TypeDenoterPtr>> nestedFields;
                        CollectOpaqueFields(nested, nestedFields);
                        for (const auto& f : nestedFields)
                        {
                            if (idx >= exprCount)
                                break;
                            s.innerOpaqueLeaves.emplace_back(prefix + vd->ident + "." + f.first, idx);
                            ++idx;
                        }
                        outSlots.push_back(std::move(s));
                    }
                    else
                    {
                        /* A nested struct with surviving POD members is flattened in place. */
                        walk(nested, prefix + vd->ident + ".");
                    }
                }
                else
                {
                    InitSlot s;
                    s.kind      = InitSlot::Kind::Pod;
                    s.exprIndex = idx;
                    outSlots.push_back(std::move(s));
                    ++idx;
                }
            }
        }
    };
    walk(structDecl, "");
}


/* ----- Pass 1: rewrite function signatures ----- */

void OpaqueStructResolver::SplitOpaqueParameter(FunctionDecl& funcDecl, std::size_t actualIndex, std::size_t logicalIndex, FunctionRewriteInfo& info)
{
    auto& param = funcDecl.parameters[actualIndex];
    auto structDecl = TryGetOpaqueStructDeclaration(param->typeSpecifier->typeDenoter);
    if (!structDecl)
        return;

    /* Collect the opaque fields of the struct in declaration order. */
    std::vector<std::pair<std::string, TypeDenoterPtr>> opaqueFields;
    CollectOpaqueFields(structDecl, opaqueFields);
    if (opaqueFields.empty())
        return;

    /* Generate one new parameter VarDeclStmnt per opaque field. */
    std::vector<OpaqueParam> newOpaqueParams;
    newOpaqueParams.reserve(opaqueFields.size());

    /* Pick a base identifier from the original parameter's first VarDecl (parameters
       always have exactly one VarDecl in HLSL grammar). */
    const std::string baseIdent = (!param->varDecls.empty() ? param->varDecls.front()->ident : std::string("p"));

    /* The synthesized parameters will be inserted right after the original parameter. */
    std::vector<VarDeclStmntPtr> newParamStmnts;
    newParamStmnts.reserve(opaqueFields.size());

    for (const auto& field : opaqueFields)
    {
        /* Build a fresh type denoter for the new parameter from the opaque field's type. */
        auto typeSpec = ASTFactory::MakeTypeSpecifier(field.second->Copy());

        /* The field path may be dotted for nested fields ("mat.albedo"); turn it into a
           valid identifier suffix ("mat_albedo"). A sibling field literally named
           "mat_albedo" would map to the same identifier, but GLSLConverter runs afterward
           and uniquifies any clashing declaration idents, so this suffix need not be
           globally unique here. */
        std::string identSuffix = field.first;
        std::replace(identSuffix.begin(), identSuffix.end(), '.', '_');
        auto stmnt = ASTFactory::MakeVarDeclStmnt(typeSpec, baseIdent + "_" + identSuffix);
        stmnt->flags << VarDeclStmnt::isParameter;
        if (!stmnt->varDecls.empty())
            newOpaqueParams.push_back(OpaqueParam{stmnt->varDecls.front().get(), field.first});
        newParamStmnts.push_back(stmnt);
    }

    /* Insert the synthesized parameter statements immediately after the original. */
    funcDecl.parameters.insert(
        funcDecl.parameters.begin() + actualIndex + 1,
        newParamStmnts.begin(), newParamStmnts.end()
    );

    /* Record the rewrite info under the LOGICAL (original) parameter slot. */
    if (info.opaqueParamsPerOriginal.size() <= logicalIndex)
        info.opaqueParamsPerOriginal.resize(logicalIndex + 1);
    info.opaqueParamsPerOriginal[logicalIndex] = std::move(newOpaqueParams);

    /* (Alias maps for these parameters are seeded later in VisitFunctionDecl when we
       enter the function body, so that they are live only inside that body.) */
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

        bool anyRewrite = false;
        /* Iterate over the ORIGINAL slot positions only; do not visit the synthesized
           parameters that get appended (they cannot be opaque-bearing structs).
           SplitOpaqueParameter inserts the new params right after the original, so we
           track the actual position with a running offset rather than recomputing it. */
        std::size_t actualIndex = 0;
        for (std::size_t i = 0; i < info.originalParamCount; ++i)
        {
            if (actualIndex >= fd.parameters.size())
                break;

            auto& param = fd.parameters[actualIndex];

            /* Pure 'out' parameters are not split: the callee cannot legally read
               their opaque leaves before writing them, so the caller has nothing to
               pass in. Their alias maps are seeded all-Unset when the body is
               processed, and the exit state flows back to the caller through the
               function summary. 'inout' parameters (IsInput() && IsOutput()) are
               split like by-value ones, since the callee may read them. */
            const bool isPureOut = (param->IsOutput() && !param->IsInput());
            if (!isPureOut && TryGetOpaqueStructDeclaration(param->typeSpecifier->typeDenoter))
            {
                SplitOpaqueParameter(fd, actualIndex, i, info);
                anyRewrite = true;
            }

            /* Advance past this original parameter and any opaque params it spawned. */
            actualIndex += 1 + info.opaqueParamsPerOriginal[i].size();
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

bool OpaqueStructResolver::ResolveFieldChain(ObjectExpr* obj, VarDecl*& outVar, std::string& outPath)
{
    if (!obj || !obj->prefixExpr)
        return false;

    auto prefixObj = obj->prefixExpr->As<ObjectExpr>();
    if (!prefixObj)
        return false;

    /* Base case: the prefix is a direct reference to a tracked local/parameter. */
    if (auto v = prefixObj->FetchVarDecl())
    {
        if (FindAliasMap(v))
        {
            outVar  = v;
            outPath = obj->ident;
            return true;
        }
    }

    /* Recursive case: the prefix is itself a field chain (e.g. `s.inner.tex`). The
       intermediate member VarDecls are not tracked locals, so the base case above
       falls through for them and we descend until we reach the rooted local. */
    VarDecl* baseVar = nullptr;
    std::string basePath;
    if (ResolveFieldChain(prefixObj, baseVar, basePath))
    {
        outVar  = baseVar;
        outPath = basePath + "." + obj->ident;
        return true;
    }

    return false;
}

bool OpaqueStructResolver::DecomposeToVarPath(Expr* expr, VarDecl*& outVar, std::string& outPath)
{
    if (!expr)
        return false;

    if (auto bracket = expr->As<BracketExpr>())
        return DecomposeToVarPath(bracket->expr.get(), outVar, outPath);

    auto obj = expr->As<ObjectExpr>();
    if (!obj)
        return false;

    if (!obj->prefixExpr)
    {
        /* Whole-variable reference (e.g. `s`). */
        if (auto v = obj->FetchVarDecl())
        {
            if (FindAliasMap(v))
            {
                outVar = v;
                outPath.clear();
                return true;
            }
        }
        return false;
    }

    /* Sub-struct field chain (e.g. passing `s.mat`). */
    return ResolveFieldChain(obj, outVar, outPath);
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
    switch (it->second.state)
    {
        case AliasEntry::State::Resolved:
            return it->second.target;
        case AliasEntry::State::Ambiguous:
            RuntimeErr(R_OpaqueStructAmbiguousAlias(fieldName), errorContext);
            return nullptr;
        case AliasEntry::State::Unset:
            RuntimeErr(R_OpaqueStructUninitialized(fieldName), errorContext);
            return nullptr;
    }
    return nullptr;
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

/* If `obj` terminates a member-access chain whose ROOT is a function call (e.g. the
   `tex` in `makeBundle().tex`, possibly through brackets), returns that CallExpr and
   the dotted field path from the call result to `obj` ("tex", "albedo.tex", ...).
   Returns null if the chain is rooted at anything else (variable, literal, ...). */
static CallExpr* FindFieldChainRootCall(ObjectExpr* obj, std::string& outPath)
{
    outPath = obj->ident;
    Expr* prefix = obj->prefixExpr.get();
    while (prefix)
    {
        if (auto bracket = prefix->As<BracketExpr>())
        {
            prefix = bracket->expr.get();
            continue;
        }
        if (auto prefixObj = prefix->As<ObjectExpr>())
        {
            outPath = prefixObj->ident + "." + outPath;
            prefix = prefixObj->prefixExpr.get();
            continue;
        }
        break;
    }
    return (prefix ? prefix->As<CallExpr>() : nullptr);
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

    /* Case A: an initializer whose opaque aliases are statically computable: a copy
       from another tracked variable or sub-struct (`Combined c2 = c;`,
       `Material m = s.mat;`), a call returning an opaque-bearing struct
       (`Bundle b = make();` -- the call was visited just before this and its
       translated return aliases recorded), or a ternary over such values. */
    {
        AliasMap computed;
        if (GetOrComputeAliasMapForExpression(initializer, structDecl, computed))
        {
            for (auto& kv : computed)
                m[kv.first] = kv.second;
            activeAliasMaps_[localVar] = std::move(m);
            return;
        }
    }

    /* Case B: flat aggregate-initializer { expr0, expr1, ... } matching struct layout.
       Nested opaque-bearing struct members contribute their leaves flattened into the
       same list (HLSL's flat aggregate form; nested-brace form is not supported by the
       generator for struct members and is left to assignment-based tracking). The slot
       layout comes from BuildInitializerSlots -- the same traversal the residual-stripping
       walk in VisitVarDeclStmnt uses -- so seeding and stripping cannot drift apart. */
    if (auto initExpr = initializer->As<InitializerExpr>())
    {
        const auto& exprs = initExpr->exprs;
        std::vector<InitSlot> slots;
        BuildInitializerSlots(structDecl, exprs.size(), slots);
        for (const auto& slot : slots)
        {
            switch (slot.kind)
            {
                case InitSlot::Kind::Opaque:
                    if (auto target = ResolveOpaqueExprToDecl(exprs[slot.exprIndex].get()))
                        m[slot.path] = AliasEntry::MakeResolved(target);
                    break;
                case InitSlot::Kind::EmptyNestedDummy:
                    for (const auto& leaf : slot.innerOpaqueLeaves)
                    {
                        if (auto target = ResolveOpaqueExprToDecl(exprs[leaf.second].get()))
                            m[leaf.first] = AliasEntry::MakeResolved(target);
                    }
                    break;
                case InitSlot::Kind::Pod:
                    break;  // POD slot: nothing to seed
            }
        }
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
            /* Present on only one side: not resolvable after the join. */
            kv.second = AliasEntry::MakeAmbiguous();
            continue;
        }

        const AliasEntry& other = it->second;
        const bool sameResolved =
            kv.second.state == AliasEntry::State::Resolved &&
            other.state     == AliasEntry::State::Resolved &&
            kv.second.target == other.target;
        const bool bothUnset =
            kv.second.state == AliasEntry::State::Unset &&
            other.state     == AliasEntry::State::Unset;

        /* Keep only if both sides agree (same resolved global, or both still unset);
           any other combination is ambiguous at the merge point. */
        if (!sameResolved && !bothUnset)
            kv.second = AliasEntry::MakeAmbiguous();
    }
    return r;
}

void OpaqueStructResolver::JoinCommonInto(AliasState& dst, const AliasState& src)
{
    for (auto& kv : dst)
    {
        auto it = src.find(kv.first);
        if (it != src.end())
            kv.second = JoinAliasMaps(kv.second, it->second);
    }
}

void OpaqueStructResolver::JoinStateInto(AliasState& dst, const AliasState& src)
{
    JoinCommonInto(dst, src);
    for (const auto& kv : src)
    {
        if (dst.find(kv.first) == dst.end())
            dst[kv.first] = kv.second;
    }
}

void OpaqueStructResolver::CopyAliasSubtree(
    AliasMap& destMap, const std::string& destPath,
    const AliasMap& srcMap, const std::string& srcPath)
{
    const std::string destPrefix = (destPath.empty() ? std::string() : destPath + ".");
    const std::string srcPrefix  = (srcPath.empty()  ? std::string() : srcPath  + ".");
    for (auto& kv : destMap)
    {
        const std::string& destKey = kv.first;
        /* Restrict to the opaque leaves under destPath. */
        if (!destPrefix.empty() && destKey.compare(0, destPrefix.size(), destPrefix) != 0)
            continue;
        /* Map the suffix below destPath to the matching leaf below srcPath. */
        const std::string suffix = (destPrefix.empty() ? destKey : destKey.substr(destPrefix.size()));
        auto it = srcMap.find(srcPrefix + suffix);
        if (it != srcMap.end())
            kv.second = it->second;
    }
}

bool OpaqueStructResolver::GetOrComputeAliasMapForExpression(Expr* expr, StructDecl* structDecl, AliasMap& outMap)
{
    if (!expr || !structDecl)
        return false;

    if (auto bracket = expr->As<BracketExpr>())
        return GetOrComputeAliasMapForExpression(bracket->expr.get(), structDecl, outMap);

    /* Case 1: a (whole or sub-) reference to a tracked local/parameter. Extract the
       entries under the sub-path, re-keyed relative to `structDecl`'s opaque leaves
       (missing entries default to Unset). */
    {
        VarDecl* srcVar = nullptr;
        std::string srcPath;
        if (DecomposeToVarPath(expr, srcVar, srcPath))
        {
            if (auto* srcMap = FindAliasMap(srcVar))
            {
                std::vector<std::pair<std::string, TypeDenoterPtr>> fields;
                CollectOpaqueFields(structDecl, fields);
                const std::string keyPrefix = (srcPath.empty() ? std::string() : srcPath + ".");
                for (const auto& f : fields)
                {
                    auto it = srcMap->find(keyPrefix + f.first);
                    outMap[f.first] = (it != srcMap->end() ? it->second : AliasEntry{});
                }
                return true;
            }
        }
    }

    /* Case 2: a call to a function returning an opaque-bearing struct. The call has
       already been visited at this point, so its caller-context return aliases are on
       record in callReturnAliases_. */
    if (auto call = expr->As<CallExpr>())
    {
        auto it = callReturnAliases_.find(call);
        if (it != callReturnAliases_.end())
        {
            outMap = it->second;
            return true;
        }
        return false;
    }

    /* Case 3: a ternary over two opaque-struct values: resolvable only where a field
       resolves identically on both sides (JoinAliasMaps marks the rest ambiguous). */
    if (auto ternary = expr->As<TernaryExpr>())
    {
        AliasMap thenMap, elseMap;
        if (GetOrComputeAliasMapForExpression(ternary->thenExpr.get(), structDecl, thenMap) &&
            GetOrComputeAliasMapForExpression(ternary->elseExpr.get(), structDecl, elseMap))
        {
            outMap = JoinAliasMaps(thenMap, elseMap);
            return true;
        }
        return false;
    }

    return false;
}

OpaqueStructResolver::AliasMap OpaqueStructResolver::RemapAliasesCalleeToCaller(const AliasMap& calleeMap, const std::unordered_map<VarDecl*, Decl*>& paramBindings)
{
    AliasMap r;
    for (const auto& kv : calleeMap)
    {
        const AliasEntry& e = kv.second;
        if (e.state == AliasEntry::State::Resolved && e.target != nullptr && e.target->Type() == AST::Types::VarDecl)
        {
            /* A VarDecl target is one of the callee's opaque parameters; map it to
               whatever the caller passed for it. A parameter the caller did not bind
               (e.g. default arguments) cannot be resolved here. */
            auto it = paramBindings.find(static_cast<VarDecl*>(e.target));
            if (it != paramBindings.end() && it->second != nullptr)
                r[kv.first] = AliasEntry::MakeResolved(it->second);
            else
                r[kv.first] = AliasEntry::MakeAmbiguous();
        }
        else
        {
            /* Global BufferDecl/SamplerDecl targets, Unset and Ambiguous states are
               valid in any context and pass through unchanged. */
            r[kv.first] = e;
        }
    }
    return r;
}

void OpaqueStructResolver::RemapAliasMap(AliasMap& destMap, const std::string& destPath, const AliasMap& srcMap)
{
    const std::string prefix = (destPath.empty() ? std::string() : destPath + ".");
    for (const auto& kv : srcMap)
        destMap[prefix + kv.first] = kv.second;
}

void OpaqueStructResolver::AccumulateOutParamState(FunctionDecl* funcDecl)
{
    if (!funcDecl)
        return;

    for (auto& param : funcDecl->parameters)
    {
        /* Only out/inout parameters of opaque-bearing struct type carry state back. */
        if (!param->IsOutput() || param->varDecls.empty())
            continue;
        if (!TryGetOpaqueStructDeclaration(param->typeSpecifier->typeDenoter))
            continue;

        VarDecl* paramVar = param->varDecls.front().get();
        auto* m = FindAliasMap(paramVar);
        if (!m)
            continue;

        auto& summary = funcSummaries_[funcDecl];
        auto it = summary.outParamAliases.find(paramVar);
        if (it == summary.outParamAliases.end())
            summary.outParamAliases[paramVar] = *m;
        else
            it->second = JoinAliasMaps(it->second, *m);
    }
}

void OpaqueStructResolver::ProcessFunction(FunctionDecl* funcDecl)
{
    /* Each body is processed exactly once. Besides the natural program-order walk,
       VisitCallExpr requests its callee eagerly so the callee's summary exists before
       the call site is translated; the guard also breaks (illegal) recursion cycles
       and makes repeated requests cheap. */
    if (!funcDecl || processedFuncs_.find(funcDecl) != processedFuncs_.end())
        return;
    processedFuncs_.insert(funcDecl);

    /* Forward declaration: no body to process (the implementation is its own node). */
    if (!funcDecl->codeBlock)
        return;

    /* Isolate the alias state: when a callee is processed from the middle of a
       caller's body, the caller's live alias maps must neither leak into the callee
       nor be polluted by the callee's locals. */
    auto savedState = std::move(activeAliasMaps_);
    activeAliasMaps_.clear();

    /* Likewise the struct-decl tracker context must not leak in: VisitVarDeclStmnt
       skips alias seeding while InsideStructDecl(), but a global function processed
       on demand from a member-function call site is not itself a struct member. */
    std::vector<StructDecl*> savedStructStack = GetStructDeclStack();
    for (std::size_t n = savedStructStack.size(); n > 0; --n)
        PopStructDecl();

    /* Seed alias maps for opaque-bearing struct parameters of this function from the
       signature rewriting that already happened. */
    auto it = funcRewrites_.find(funcDecl);
    if (it != funcRewrites_.end())
    {
        const auto& info = it->second;
        std::size_t actualIndex = 0;
        for (std::size_t i = 0; i < info.originalParamCount; ++i)
        {
            if (actualIndex >= funcDecl->parameters.size())
                break;
            auto& origParam = funcDecl->parameters[actualIndex];
            const auto& opaques = info.opaqueParamsPerOriginal[i];
            if (!origParam->varDecls.empty() && !opaques.empty())
            {
                VarDecl* localVar = origParam->varDecls.front().get();
                AliasMap m;
                for (const auto& op : opaques)
                    m[op.field] = AliasEntry::MakeResolved(op.param);
                activeAliasMaps_[localVar] = std::move(m);
            }
            /* Advance past this original + any opaque params it spawned. */
            actualIndex += 1 + opaques.size();
        }
    }

    /* Pure 'out' opaque-struct parameters were not split, but their opaque leaves
       still need an alias map so writes in this body are tracked (and the statements
       dropped). They start out Unset, matching HLSL's uninitialized-on-entry 'out'
       semantics: a read before a write is an error. */
    for (auto& param : funcDecl->parameters)
    {
        if (!param->IsOutput() || param->IsInput() || param->varDecls.empty())
            continue;
        if (auto structDecl = TryGetOpaqueStructDeclaration(param->typeSpecifier->typeDenoter))
        {
            AliasMap m;
            std::vector<std::pair<std::string, TypeDenoterPtr>> fields;
            CollectOpaqueFields(structDecl, fields);
            for (const auto& f : fields)
                m[f.first] = AliasEntry{};
            activeAliasMaps_[param->varDecls.front().get()] = std::move(m);
        }
    }

    PushFunctionDecl(funcDecl);
    Visit(funcDecl->codeBlock);
    PopFunctionDecl();

    /* Fall-through exit (void functions, or a body whose last statement is not a
       return): fold the final state of out/inout opaque-struct parameters into their
       exit summaries. For bodies ending in a return this is an idempotent re-join. */
    AccumulateOutParamState(funcDecl);

    /* Restore the caller's context. */
    for (auto* sd : savedStructStack)
        PushStructDecl(sd);
    activeAliasMaps_ = std::move(savedState);
}


#define IMPLEMENT_VISIT_PROC(AST_NAME) \
    void OpaqueStructResolver::Visit##AST_NAME(AST_NAME* ast, void* args)


IMPLEMENT_VISIT_PROC(StructDecl)
{
    /* Track struct-decl context so VisitVarDeclStmnt does not mistake opaque-bearing
       struct *members* (e.g. a nested `TexBundle albedo;`) for local variables and seed
       bogus alias maps for them. Member functions in localStmnts are still visited. */
    PushStructDecl(ast);
    {
        VISIT_DEFAULT(StructDecl);
    }
    PopStructDecl();
}

IMPLEMENT_VISIT_PROC(FunctionDecl)
{
    /* All body processing (parameter alias seeding, body walk, summary capture)
       lives in ProcessFunction so that call sites can request a callee on demand;
       processing is memoized, so this is a no-op for bodies already handled. */
    ProcessFunction(ast);
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
    if (auto structDecl = TryGetOpaqueStructDeclaration(ast->typeSpecifier->typeDenoter))
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
                        /* Rebuild the flat aggregate initializer keeping only the POD
                           residual. Opaque leaves are dropped; a fully-opaque nested
                           struct collapses to the single dummy int the generator adds for
                           an empty struct (see GLSLConverter), so we emit one `0` in its
                           place. The slot layout comes from BuildInitializerSlots -- the
                           same traversal used to seed the alias map -- so stripping stays
                           in lockstep with seeding by construction. */
                        const auto& exprs = initExpr->exprs;
                        std::vector<InitSlot> slots;
                        BuildInitializerSlots(structDecl, exprs.size(), slots);
                        std::vector<ExprPtr> kept;
                        for (const auto& slot : slots)
                        {
                            switch (slot.kind)
                            {
                                case InitSlot::Kind::Opaque:
                                    break;  // drop opaque leaf
                                case InitSlot::Kind::Pod:
                                    kept.push_back(exprs[slot.exprIndex]);
                                    break;
                                case InitSlot::Kind::EmptyNestedDummy:
                                    kept.push_back(ASTFactory::MakeLiteralExpr(DataType::Int, "0"));
                                    break;
                            }
                        }
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
                        /* Non-aggregate initializer. A copy-init from another struct
                           variable carries no residual data if the struct is fully
                           opaque, so it is dropped. A call initializer
                           (`Bundle b = make();`) is always kept: the callee still
                           returns the (possibly dummy) POD residual and may have side
                           effects. */
                        Expr* core = vd->initializer.get();
                        while (auto bracket = core->As<BracketExpr>())
                            core = bracket->expr.get();
                        if (core->Type() != AST::Types::CallExpr && StructIsFullyOpaque(structDecl))
                            vd->initializer.reset();
                    }
                }
            }
        }
    }
}

IMPLEMENT_VISIT_PROC(ExprStmnt)
{
    if (auto ax = ast->expr ? ast->expr->As<AssignExpr>() : nullptr)
    {
        if (auto lhsObj = ax->lvalueExpr ? ax->lvalueExpr->As<ObjectExpr>() : nullptr)
        {
            /* Resolve the assignment target to a tracked (sub-)struct: (destVar, destPath),
               where destPath is the dotted path within destVar ("" = the whole variable). */
            VarDecl* destVar = nullptr;
            std::string destPath;
            bool lhsResolved = DecomposeToVarPath(lhsObj, destVar, destPath);

            if (lhsResolved)
            {
                if (auto* m = FindAliasMap(destVar))
                {
                    /* Case 1: straight-line opaque-field assignment
                       `localVar.[a.b...].opaqueField = rhs;`. The opaque member vanishes
                       after stripping, so update the alias map and drop the statement. */
                    if (m->find(destPath) != m->end())
                    {
                        if (auto target = ResolveOpaqueExprToDecl(ax->rvalueExpr.get()))
                            (*m)[destPath] = AliasEntry::MakeResolved(target);
                        else
                            (*m)[destPath] = AliasEntry::MakeAmbiguous();

                        /* Replace the whole expression with a NullExpr so downstream passes
                           (ExprConverter) don't traverse into ObjectExprs that reference
                           soon-to-be-stripped struct members, and mark the stmnt dead so
                           RemoveDeadCode drops it from its CodeBlock. Replacing the entire
                           expr (rather than just nulling the operands) also degrades to a
                           valid empty statement `;` in the case RemoveDeadCode cannot reach
                           -- an unbraced single-statement branch/loop body, which is not held
                           in a statement list. */
                        ast->expr = std::make_shared<NullExpr>(SourcePosition::ignore);
                        ast->flags << AST::isDeadCode;
                        return;
                    }

                    /* Case 2: whole-struct or sub-struct copy assignment
                       `dst = src;`, `dst.sub = src;` or `dst.sub = other.sub;`. The opaque
                       leaves under destPath inherit the source's aliases; any POD residual is
                       copied by the surviving struct assignment, so the statement is kept and
                       emitted normally (only the opaque leaves were tracked here). */
                    VarDecl* srcVar = nullptr;
                    std::string srcPath;
                    if (DecomposeToVarPath(ax->rvalueExpr.get(), srcVar, srcPath))
                    {
                        if (auto* srcMap = FindAliasMap(srcVar))
                        {
                            CopyAliasSubtree(*m, destPath, *srcMap, srcPath);
                            return;
                        }
                    }

                    /* Case 3: whole-struct or sub-struct assignment from a call that
                       returns an opaque-bearing struct (`dst = makeBundle();`,
                       `dst.sub = makeBundle();`) or from a ternary over opaque-struct
                       values. Visit the right-hand side first so the call is rewritten
                       and its translated return aliases recorded, then adopt those
                       aliases for the opaque leaves under destPath. The statement is
                       kept: the emitted assignment still copies the POD residual. */
                    Expr* rhsCore = ax->rvalueExpr.get();
                    while (rhsCore)
                    {
                        if (auto bracket = rhsCore->As<BracketExpr>())
                        {
                            rhsCore = bracket->expr.get();
                            continue;
                        }
                        break;
                    }
                    if (rhsCore && (rhsCore->Type() == AST::Types::CallExpr || rhsCore->Type() == AST::Types::TernaryExpr))
                    {
                        if (auto destStruct = TryGetOpaqueStructDeclaration(lhsObj->GetTypeDenoter()))
                        {
                            Visit(ax->rvalueExpr);

                            AliasMap computed;
                            if (GetOrComputeAliasMapForExpression(ax->rvalueExpr.get(), destStruct, computed))
                                RemapAliasMap(*m, destPath, computed);
                            else
                            {
                                /* Unresolvable right-hand side: poison the leaves under
                                   destPath so any later read reports a clear error. */
                                std::vector<std::pair<std::string, TypeDenoterPtr>> fields;
                                CollectOpaqueFields(destStruct, fields);
                                const std::string prefix = (destPath.empty() ? std::string() : destPath + ".");
                                for (const auto& f : fields)
                                    (*m)[prefix + f.first] = AliasEntry::MakeAmbiguous();
                            }

                            /* The rhs has already been visited; returning here prevents
                               the default fall-through visit from rewriting the call's
                               arguments a second time. */
                            return;
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
    /* Detect an opaque field-access chain `localVar.[a.b...].opaqueField` and rewrite it
       in-place to a direct reference to the resolved global Decl. Only opaque fields are
       in the alias map, so POD member access (e.g. `tc.tint` or `s.mat` as a whole)
       falls through to a normal prefix visit unchanged. */
    if (ast->prefixExpr)
    {
        VarDecl* localVar = nullptr;
        std::string path;
        if (ResolveFieldChain(ast, localVar, path))
        {
            if (auto* m = FindAliasMap(localVar))
            {
                if (m->find(path) != m->end())
                {
                    if (auto target = ResolveOpaqueFieldAccess(localVar, path, ast))
                    {
                        ast->ReplaceSymbol(target);
                        ast->prefixExpr.reset();
                    }
                    /* Matched an opaque field; the prefix chain is consumed (or an error
                       was already raised). Do not visit the discarded prefix. */
                    return;
                }
            }
        }

        /* A field chain rooted at a function call that returns an opaque-bearing
           struct (e.g. `makeBundle().tex`): resolving the opaque leaf here would have
           to discard the call itself, silently dropping its side effects. Reject with
           a dedicated diagnostic; POD residual access (`makeBundle().tint`) and
           swizzles on constructor calls are unaffected (their path does not name an
           opaque leaf, or the callee does not return an opaque-bearing struct). */
        {
            std::string path;
            if (auto rootCall = FindFieldChainRootCall(ast, path))
            {
                auto callee = rootCall->GetFunctionImpl();
                if (!callee)
                    callee = rootCall->GetFunctionDecl();
                if (callee)
                {
                    if (auto retStruct = TryGetOpaqueStructDeclaration(callee->returnType->typeDenoter))
                    {
                        std::vector<std::pair<std::string, TypeDenoterPtr>> fields;
                        CollectOpaqueFields(retStruct, fields);
                        for (const auto& f : fields)
                        {
                            if (f.first == path)
                                RuntimeErr(R_OpaqueStructCallFieldAccess(path), ast);
                        }
                    }
                }
            }
        }

        /* Not an opaque field access: visit the prefix for any nested rewrites. */
        Visit(ast->prefixExpr);
    }
}

IMPLEMENT_VISIT_PROC(CallExpr)
{
    /* Visit prefix and arguments first to rewrite any contained ObjectExprs. */
    if (ast->prefixExpr)
        Visit(ast->prefixExpr);
    for (auto& a : ast->arguments)
        Visit(a);

    auto funcDecl = ast->GetFunctionImpl();
    if (!funcDecl)
        funcDecl = ast->GetFunctionDecl();
    if (!funcDecl)
        return;

    auto it = funcRewrites_.find(funcDecl);
    const FunctionRewriteInfo* info = (it != funcRewrites_.end() ? &it->second : nullptr);

    const bool returnsOpaqueStruct = (TryGetOpaqueStructDeclaration(funcDecl->returnType->typeDenoter) != nullptr);

    bool hasOpaqueOutParam = false;
    for (const auto& param : funcDecl->parameters)
    {
        if (param->IsOutput() && TryGetOpaqueStructDeclaration(param->typeSpecifier->typeDenoter))
        {
            hasOpaqueOutParam = true;
            break;
        }
    }

    /* Nothing to do for calls that involve no opaque-bearing structs at all. */
    if (!info && !returnsOpaqueStruct && !hasOpaqueOutParam)
        return;

    /* The callee's return-value / out-parameter summaries are produced while its body
       is processed. Bodies are normally processed in program order, so force the
       callee now (memoized: each body is still rewritten exactly once), covering
       callees defined after their callers and forward declarations. */
    if (returnsOpaqueStruct || hasOpaqueOutParam)
        ProcessFunction(funcDecl);

    /* Caller-context binding of each callee opaque parameter -- both the synthesized
       opaque-field params and plain opaque-typed params (e.g. "Texture2D t") -- used
       to translate the callee's summary targets into this call site. */
    std::unordered_map<VarDecl*, Decl*> paramBindings;

    /* Out/inout opaque-struct argument slots, for post-call alias copy-back. */
    std::vector<std::pair<Expr*, VarDecl*>> outArgs;

    static const std::vector<OpaqueParam> noOpaques;

    /* Build the new argument list. For each original-parameter slot, keep the original
       argument expr, then append one resolved-opaque arg per opaque field. */
    std::vector<ExprPtr> newArgs;
    newArgs.reserve(ast->arguments.size());

    const std::size_t originalParamCount =
        (info ? info->originalParamCount : funcDecl->parameters.size());

    /* If there are fewer args than original params, the remainder are default arguments
       (the reference analyzer attaches defaultArgumentRefs); we only decompose explicit
       arguments, so the loop below naturally stops at ast->arguments.size(). */
    std::size_t argIdx         = 0;
    std::size_t actualParamIdx = 0;
    for (std::size_t i = 0; i < originalParamCount; ++i)
    {
        if (argIdx >= ast->arguments.size())
            break;
        ExprPtr origArg = ast->arguments[argIdx++];
        newArgs.push_back(origArg);

        auto param    = (actualParamIdx < funcDecl->parameters.size() ? funcDecl->parameters[actualParamIdx].get() : nullptr);
        auto paramVar = (param && !param->varDecls.empty() ? param->varDecls.front().get() : nullptr);

        /* Record the binding of a plain opaque-typed parameter, so callee summary
           entries resolved to it translate back to this argument's global. */
        if (param && paramVar && Converter::IsOpaqueTypeDenoter(param->typeSpecifier->GetTypeDenoter()))
        {
            if (auto target = ResolveOpaqueExprToDecl(origArg.get()))
                paramBindings[paramVar] = target;
        }

        const auto& opaques =
            (info && i < info->opaqueParamsPerOriginal.size() ? info->opaqueParamsPerOriginal[i] : noOpaques);
        if (!opaques.empty())
        {
            /* Resolve each opaque field of the arg via the alias map of the local
               opaque-bearing struct variable (or sub-struct) passed as the argument.
               When a sub-struct is passed (e.g. `s.mat`), `basePath` is the access path
               within the local and the callee's field paths are looked up relative to it. */
            VarDecl* localVar = nullptr;
            std::string basePath;
            if (DecomposeToVarPath(origArg.get(), localVar, basePath))
            {
                for (const auto& op : opaques)
                {
                    const auto& fieldName = op.field;
                    const std::string key = (basePath.empty() ? fieldName : basePath + "." + fieldName);

                    /* Resolve through the same diagnostic path as a direct field access, so a
                       failure reports "read before assigned" vs. "ambiguous after a branch"
                       consistently (ResolveOpaqueFieldAccess raises the error and returns null). */
                    Decl* target = ResolveOpaqueFieldAccess(localVar, key, ast);
                    if (!target)
                        return;

                    paramBindings[op.param] = target;
                    newArgs.push_back(ASTFactory::MakeObjectExpr(target));
                }
            }
            else
            {
                /* The argument is not a reference to a tracked variable: it may be a
                   call returning an opaque-bearing struct passed straight through
                   (`helper(makeBundle(), uv)`) or a ternary over such values. */
                AliasMap argAliases;
                auto paramStruct = (param ? TryGetOpaqueStructDeclaration(param->typeSpecifier->typeDenoter) : nullptr);
                if (!paramStruct || !GetOrComputeAliasMapForExpression(origArg.get(), paramStruct, argAliases))
                    RuntimeErr(R_OpaqueStructUninitialized(opaques.front().field), ast);

                for (const auto& op : opaques)
                {
                    auto fIt = argAliases.find(op.field);
                    const AliasEntry entry = (fIt != argAliases.end() ? fIt->second : AliasEntry{});
                    if (entry.state == AliasEntry::State::Ambiguous)
                        RuntimeErr(R_OpaqueStructAmbiguousAlias(op.field), ast);
                    if (entry.state != AliasEntry::State::Resolved || entry.target == nullptr)
                        RuntimeErr(R_OpaqueStructUninitialized(op.field), ast);

                    paramBindings[op.param] = entry.target;
                    newArgs.push_back(ASTFactory::MakeObjectExpr(entry.target));
                }
            }
        }

        /* Remember out/inout opaque-struct arguments; the callee's exit-state flows
           back into the caller's alias map for this argument after the call. */
        if (param && paramVar && param->IsOutput() && TryGetOpaqueStructDeclaration(param->typeSpecifier->typeDenoter))
            outArgs.emplace_back(origArg.get(), paramVar);

        actualParamIdx += 1 + opaques.size();
    }
    if (info)
        ast->arguments = std::move(newArgs);

    auto summaryIt = funcSummaries_.find(funcDecl);

    /* Record this call's return-value aliases (translated into caller context) so the
       consumers of the call result -- initializer, assignment, enclosing return, or a
       direct pass-through argument -- can seed or update alias maps from them. */
    if (returnsOpaqueStruct)
    {
        AliasMap translated;
        if (summaryIt != funcSummaries_.end() && summaryIt->second.hasReturnAliases)
            translated = RemapAliasesCalleeToCaller(summaryIt->second.returnAliases, paramBindings);
        else
        {
            /* No usable summary (e.g. body unavailable): nothing can be resolved. */
            std::vector<std::pair<std::string, TypeDenoterPtr>> fields;
            CollectOpaqueFields(TryGetOpaqueStructDeclaration(funcDecl->returnType->typeDenoter), fields);
            for (const auto& f : fields)
                translated[f.first] = AliasEntry::MakeAmbiguous();
        }
        callReturnAliases_[ast] = std::move(translated);
    }

    /* Copy the callee's exit-state of each out/inout opaque-struct parameter back into
       the caller's alias map for the corresponding argument variable. */
    for (const auto& oa : outArgs)
    {
        AliasMap translated;
        bool haveSummary = false;
        if (summaryIt != funcSummaries_.end())
        {
            auto opIt = summaryIt->second.outParamAliases.find(oa.second);
            if (opIt != summaryIt->second.outParamAliases.end())
            {
                translated  = RemapAliasesCalleeToCaller(opIt->second, paramBindings);
                haveSummary = true;
            }
        }
        if (!haveSummary)
        {
            /* No exit summary (e.g. body unavailable): the argument's bindings are
               unknown after the call. */
            std::vector<std::pair<std::string, TypeDenoterPtr>> fields;
            CollectOpaqueFields(TryGetOpaqueStructDeclaration(oa.second->GetTypeDenoter()), fields);
            for (const auto& f : fields)
                translated[f.first] = AliasEntry::MakeAmbiguous();
        }

        VarDecl* destVar = nullptr;
        std::string destPath;
        if (!DecomposeToVarPath(oa.first, destVar, destPath))
            RuntimeErr(R_OpaqueStructOutArgUntracked(oa.second->ident.Original()), ast);

        if (auto* m = FindAliasMap(destVar))
            RemapAliasMap(*m, destPath, translated);
    }
}

IMPLEMENT_VISIT_PROC(ReturnStmnt)
{
    Visit(ast->attribs);
    Visit(ast->expr);

    auto funcDecl = ActiveFunctionDecl();
    if (!funcDecl)
        return;

    /* If this function returns an opaque-bearing struct, record how the returned
       value's opaque leaves resolve at this return point. Multiple return statements
       are joined: a field bound differently on two return paths becomes ambiguous
       (and is rejected only if a caller actually reads it). */
    if (ast->expr)
    {
        if (auto retStruct = TryGetOpaqueStructDeclaration(funcDecl->returnType->typeDenoter))
        {
            AliasMap returned;
            if (!GetOrComputeAliasMapForExpression(ast->expr.get(), retStruct, returned))
            {
                /* Unresolvable return expression (e.g. a cast): poison all leaves. */
                std::vector<std::pair<std::string, TypeDenoterPtr>> fields;
                CollectOpaqueFields(retStruct, fields);
                for (const auto& f : fields)
                    returned[f.first] = AliasEntry::MakeAmbiguous();
            }

            auto& summary = funcSummaries_[funcDecl];
            if (!summary.hasReturnAliases)
            {
                summary.returnAliases    = std::move(returned);
                summary.hasReturnAliases = true;
            }
            else
                summary.returnAliases = JoinAliasMaps(summary.returnAliases, returned);
        }
    }

    /* Every return statement is a function exit: fold the current state of out/inout
       opaque-struct parameters into their exit summaries. */
    AccumulateOutParamState(funcDecl);
}

IMPLEMENT_VISIT_PROC(IfStmnt)
{
    if (ast->condition)
        Visit(ast->condition);

    /* Evaluate the then-branch from a snapshot, then the else-branch from the same
       snapshot, and join their end-states: a field that diverges becomes ambiguous, and
       a local first bound in only one branch is carried over (JoinStateInto). With no
       else clause the else-state is just the snapshot, so a one-armed rebind diverges. */
    auto before = activeAliasMaps_;
    Visit(ast->bodyStmnt);
    auto afterThen = activeAliasMaps_;

    activeAliasMaps_ = before;
    if (ast->elseStmnt)
        Visit(ast->elseStmnt);

    JoinStateInto(activeAliasMaps_, afterThen);
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
    JoinCommonInto(activeAliasMaps_, before);
}

IMPLEMENT_VISIT_PROC(WhileLoopStmnt)
{
    if (ast->condition)
        Visit(ast->condition);

    auto before = activeAliasMaps_;
    Visit(ast->bodyStmnt);
    JoinCommonInto(activeAliasMaps_, before);
}

IMPLEMENT_VISIT_PROC(DoWhileLoopStmnt)
{
    auto before = activeAliasMaps_;
    Visit(ast->bodyStmnt);
    if (ast->condition)
        Visit(ast->condition);
    JoinCommonInto(activeAliasMaps_, before);
}

IMPLEMENT_VISIT_PROC(SwitchStmnt)
{
    if (ast->selector)
        Visit(ast->selector);

    /* Each case is evaluated from the same pre-switch snapshot; their end-states are
       joined across cases (a field that differs between cases becomes ambiguous). */
    auto before = activeAliasMaps_;
    AliasState accum;
    bool first = true;
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
            JoinCommonInto(accum, activeAliasMaps_);
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
