/*
 * OpaqueProgramPlan.cpp
 */

#include "OpaqueProgramPlan.h"
#include "AST.h"


namespace Xsc
{

namespace OpaqueLowering
{


/* ----- Binding comparison and joining ----- */

bool SameBinding(const OpaqueBinding& lhs, const OpaqueBinding& rhs)
{
    if (lhs.kind != rhs.kind || lhs.resource != rhs.resource || lhs.formal != rhs.formal || lhs.laneIndex != rhs.laneIndex)
        return false;
    if (lhs.indices.size() != rhs.indices.size() || lhs.elements.size() != rhs.elements.size())
        return false;
    for (std::size_t i = 0; i < lhs.indices.size(); ++i)
    {
        const auto& a = lhs.indices[i];
        const auto& b = rhs.indices[i];
        if (a.dynamic != b.dynamic)
            return false;
        if (a.dynamic ? a.expression.get() != b.expression.get() : a.constant != b.constant)
            return false;
    }
    for (std::size_t i = 0; i < lhs.elements.size(); ++i)
    {
        if (!SameBinding(lhs.elements[i], rhs.elements[i]))
            return false;
    }
    return true;
}

OpaqueBinding JoinBinding(const OpaqueBinding& lhs, const OpaqueBinding& rhs)
{
    return (SameBinding(lhs, rhs) ? lhs : OpaqueBinding::Conflict());
}


/* ----- Shared program helpers ----- */

FunctionDecl* CanonicalFunction(FunctionDecl* funcDecl)
{
    if (funcDecl && funcDecl->IsForwardDecl() && funcDecl->funcImplRef)
        return funcDecl->funcImplRef;
    return funcDecl;
}

void ForEachFunction(Program& program, const std::function<void(FunctionDecl*)>& callback)
{
    for (const auto& stmnt : program.globalStmnts)
    {
        auto basic = stmnt->As<BasicDeclStmnt>();
        if (!basic || !basic->declObject)
            continue;
        if (auto func = basic->declObject->As<FunctionDecl>())
            callback(func);
        else if (auto structDecl = basic->declObject->As<StructDecl>())
        {
            for (const auto& memberFunc : structDecl->funcMembers)
                callback(memberFunc.get());
        }
    }
}

const FunctionContract* FindContract(const OpaqueProgramPlan& plan, FunctionDecl* function)
{
    if (!function)
        return nullptr;
    auto it = plan.contractIndex.find(function);
    if (it != plan.contractIndex.end())
        return &plan.contracts[it->second];
    function = CanonicalFunction(function);
    it = plan.contractIndex.find(function);
    return (it != plan.contractIndex.end() ? &plan.contracts[it->second] : nullptr);
}

FunctionContract* FindContract(OpaqueProgramPlan& plan, FunctionDecl* function)
{
    return const_cast<FunctionContract*>(FindContract(static_cast<const OpaqueProgramPlan&>(plan), function));
}

const FunctionABI* FindABI(const OpaqueProgramPlan& plan, FunctionDecl* function)
{
    if (!function)
        return nullptr;
    auto it = plan.contractIndex.find(function);
    if (it != plan.contractIndex.end())
        return &plan.abis[it->second];
    function = CanonicalFunction(function);
    it = plan.contractIndex.find(function);
    return (it != plan.contractIndex.end() ? &plan.abis[it->second] : nullptr);
}


} // /namespace OpaqueLowering

} // /namespace Xsc



// ================================================================================
