/*
 * OpaqueProgramAnalyzer.cpp
 */

#include "OpaqueProgramAnalyzer.h"
#include "AST.h"
#include "Exception.h"
#include "ReportIdents.h"
#include "TypeDenoter.h"

#include <algorithm>
#include <cstdlib>
#include <string>


namespace Xsc
{

namespace OpaqueLowering
{


/* ----- Access parsing helpers (file-local) ----- */

// Parses a literal expression as a constant integer index.
static bool ParseConstantIndex(const ExprPtr& expr, int& value)
{
    auto literal = (expr ? expr->As<LiteralExpr>() : nullptr);
    if (!literal)
        return false;

    char* end = nullptr;
    const long parsed = std::strtol(literal->value.c_str(), &end, 0);
    if (end == literal->value.c_str() || (end != nullptr && *end != '\0'))
        return false;

    value = static_cast<int>(parsed);
    return true;
}

// Recursively parses an object/array access chain into an AccessDescription
// rooted in a variable; returns false for any unsupported expression shape.
static bool DescribeAccess(const ExprPtr& expr, AccessDescription& access)
{
    if (!expr)
        return false;

    if (auto bracket = expr->As<BracketExpr>())
        return DescribeAccess(bracket->expr, access);

    if (auto object = expr->As<ObjectExpr>())
    {
        if (!object->prefixExpr)
        {
            access.root = object->FetchVarDecl();
            return (access.root != nullptr);
        }

        if (!DescribeAccess(object->prefixExpr, access))
            return false;

        if (auto field = object->FetchVarDecl())
            access.constantPath.push_back(PathStep::Field(field));
        else
            return false;

        return true;
    }

    if (auto array = expr->As<ArrayExpr>())
    {
        if (!DescribeAccess(array->prefixExpr, access))
            return false;

        for (const auto& indexExpr : array->arrayIndices)
        {
            int index = 0;
            if (ParseConstantIndex(indexExpr, index))
                access.constantPath.push_back(PathStep::Index(index));
            else
            {
                DynamicStep step;
                step.position   = access.constantPath.size();
                step.expression = indexExpr;
                access.dynamicSteps.push_back(step);
                access.constantPath.push_back(PathStep::Index(0));
            }
        }

        return true;
    }

    return false;
}

// Like DescribeAccess, but resolves chains that directly reference a global
// resource declaration (BufferDecl/SamplerDecl), collecting its indices.
static bool DescribeResource(const ExprPtr& expr, Decl*& root, std::vector<IndexUse>& indices)
{
    if (!expr)
        return false;

    if (auto bracket = expr->As<BracketExpr>())
        return DescribeResource(bracket->expr, root, indices);

    if (auto object = expr->As<ObjectExpr>())
    {
        if (object->prefixExpr)
            return false;

        auto symbol = object->symbolRef;
        if (symbol && (symbol->As<BufferDecl>() || symbol->As<SamplerDecl>()))
        {
            root = symbol;
            return true;
        }

        return false;
    }

    if (auto array = expr->As<ArrayExpr>())
    {
        if (!DescribeResource(array->prefixExpr, root, indices))
            return false;

        for (const auto& indexExpr : array->arrayIndices)
        {
            IndexUse index;
            if (!ParseConstantIndex(indexExpr, index.constant))
            {
                index.dynamic    = true;
                index.expression = indexExpr;
            }

            indices.push_back(index);
        }

        return true;
    }

    return false;
}

// Returns true when an access indexes the contents of a resource object, rather than an array of resources.
// e.g. storageBuffer[index] = value . This is image store operation rather than a resource array access, same for texture reads.
static bool IsResourceDataAccess(const ExprPtr& expr)
{
    if (!expr)
        return false;
    if (expr->As<DescriptorHeapExpr>())
        return false;
    if (auto bracket = expr->As<BracketExpr>())
        return IsResourceDataAccess(bracket->expr);
    if (auto object = expr->As<ObjectExpr>())
        return IsResourceDataAccess(object->prefixExpr);
    if (auto array = expr->As<ArrayExpr>())
    {
        const auto prefixType = array->prefixExpr->GetTypeDenoter()->GetSub();
        if (prefixType->GetAliased().As<BufferTypeDenoter>())
            return true;

        return IsResourceDataAccess(array->prefixExpr);
    }
    return false;
}


/* ----- Dataflow lattice helpers (file-local) ----- */

// Builds an OpaqueValue for 'node' (default: the layout root) with all lane
// bindings defaulted to Uninitialized.
static OpaqueValue MakeUninitialized(const OpaqueTypeLayoutPtr& layout, const LayoutNodePtr& node = nullptr)
{
    OpaqueValue value;
    value.layout = layout;
    value.node   = (node ? node : (layout ? layout->root : nullptr));
    if (value.node)
        value.bindings.resize(value.node->laneIndices.size());

    return value;
}

// Lane-wise lattice join of two values; a lane count mismatch conflicts all lanes.
static OpaqueValue JoinValue(const OpaqueValue& lhs, const OpaqueValue& rhs)
{
    if (!lhs.Valid())
        return rhs;

    if (!rhs.Valid())
        return lhs;

    OpaqueValue result = lhs;
    if (lhs.bindings.size() != rhs.bindings.size())
    {
        for (auto& binding : result.bindings)
            binding = OpaqueBinding::Conflict();

        return result;
    }

    for (std::size_t i = 0; i < result.bindings.size(); ++i)
        result.bindings[i] = JoinBinding(lhs.bindings[i], rhs.bindings[i]);

    return result;
}

// Joins two environments variable-wise; variables present in only one side
// are carried over unchanged.
static BindingEnvironment JoinEnvironment(const BindingEnvironment& lhs, const BindingEnvironment& rhs)
{
    BindingEnvironment result = lhs;
    for (const auto& entry : rhs)
    {
        auto it = result.find(entry.first);
        if (it != result.end())
            it->second = JoinValue(it->second, entry.second);
        else
            result[entry.first] = entry.second;
    }

    return result;
}


/* ----- OpaqueProgramAnalyzer ----- */

OpaqueProgramAnalyzer::OpaqueProgramAnalyzer(Program& program, OpaqueTypeLayoutCache& layouts, OpaqueProgramPlan& plan) :
    program_ { program }, layouts_ { layouts }, plan_ { plan }
{
}

void OpaqueProgramAnalyzer::BuildAndAnalyze()
{
    BuildContracts();

    for (auto& contract : plan_.contracts)
    {
        if (contract.declaration == contract.canonical)
            AnalyzeFunction(contract.canonical);
    }

    BuildABIs();
}

void OpaqueProgramAnalyzer::BuildContracts()
{
    ForEachFunction(program_, [&](FunctionDecl* funcDecl)
    {
        FunctionContract contract;
        contract.declaration       = funcDecl;
        contract.canonical         = CanonicalFunction(funcDecl);
        contract.originalReturnType = funcDecl->returnType;
        contract.returnLayout      = layouts_.Get(funcDecl->returnType->typeDenoter, funcDecl->returnType.get());

        if (contract.returnLayout && contract.returnLayout->IsNativeOpaqueInput())
            RuntimeErr(R_OpaqueStructNoReturn(funcDecl->returnType->typeDenoter->ToString()), funcDecl->returnType.get());

        for (const auto& param : funcDecl->parameters)
        {
            ParameterContract parameterContract;
            parameterContract.declaration = param;
            parameterContract.variable    = (!param->varDecls.empty() ? param->varDecls.front().get() : nullptr);
            parameterContract.input       = param->IsInput();
            parameterContract.output      = param->IsOutput();
            parameterContract.layout      = layouts_.Get(parameterContract.variable, param.get());
            parameterContract.nativeOpaque = (parameterContract.layout && parameterContract.layout->IsNativeOpaqueInput());

            if (parameterContract.nativeOpaque && parameterContract.output)
                RuntimeErr(R_OpaqueStructNoOutInout(parameterContract.variable ? parameterContract.variable->ident.Original() : std::string("opaque parameter")), param.get());

            if (parameterContract.nativeOpaque && parameterContract.variable)
                plan_.nativeOpaqueParameters.insert(parameterContract.variable);

            if (parameterContract.layout && parameterContract.layout->HasOpaque() && parameterContract.variable && parameterContract.variable->initializer)
                RuntimeErr(R_OpaqueTypeDefaultArgument(parameterContract.variable->ident.Original()), parameterContract.variable);

            contract.parameters.push_back(parameterContract);
        }

        plan_.contractIndex[funcDecl] = plan_.contracts.size();
        plan_.contracts.push_back(contract);
    });
}

void OpaqueProgramAnalyzer::BuildABIs()
{
    plan_.abis.clear();
    plan_.abis.reserve(plan_.contracts.size());
    for (auto& contract : plan_.contracts)
    {
        FunctionABI abi;
        abi.declaration    = contract.declaration;
        abi.lowerReturn   = (contract.returnLayout && contract.returnLayout->HasOpaque());
        abi.returnResidual = (abi.lowerReturn && contract.returnLayout->HasResidual());
        for (const auto& param : contract.parameters)
        {
            ParameterABI parameterABI;
            parameterABI.nativeOpaque = param.nativeOpaque;
            parameterABI.keepResidual = (!param.layout || !param.layout->HasOpaque() || param.layout->HasResidual() || param.nativeOpaque);
            if (param.layout && param.layout->HasOpaque() && !param.nativeOpaque && param.input)
            {
                std::unordered_set<std::size_t> groupedLanes;
                auto groupsIt = plan_.dynamicLaneGroups.find(param.variable);
                if (groupsIt != plan_.dynamicLaneGroups.end())
                {
                    parameterABI.arrayLaneGroups = groupsIt->second;
                    for (const auto& group : parameterABI.arrayLaneGroups)
                        groupedLanes.insert(group.begin(), group.end());
                }
                for (std::size_t i = 0; i < param.layout->lanes.size(); ++i)
                {
                    if (groupedLanes.find(i) == groupedLanes.end())
                        parameterABI.scalarLanes.push_back(i);
                }
            }
            abi.parameters.push_back(parameterABI);
        }
        plan_.abis.push_back(abi);
    }
}

void OpaqueProgramAnalyzer::AnalyzeFunction(FunctionDecl* funcDecl)
{
    funcDecl = CanonicalFunction(funcDecl);

    if (!funcDecl || !funcDecl->codeBlock)
        return;

    auto& summary = plan_.summaries[funcDecl];
    if (summary.analyzed)
        return;

    if (summary.analyzing)
        RuntimeErr(R_IllegalRecursiveCall(funcDecl->ToString()), funcDecl);

    summary.analyzing = true;

    FunctionContract* contract = FindContract(plan_, funcDecl);
    BindingEnvironment env;
    if (contract)
    {
        for (const auto& param : contract->parameters)
        {
            if (!param.variable || !param.layout || !param.layout->HasOpaque())
                continue;

            OpaqueValue value = MakeUninitialized(param.layout);
            if (param.input)
            {
                for (std::size_t i = 0; i < value.bindings.size(); ++i)
                    value.bindings[i] = OpaqueBinding::Formal(param.variable, i);
            }

            env[param.variable] = value;
            plan_.variableLayouts[param.variable] = param.layout;
        }
    }

    FunctionDecl* previous = activeFunction_;
    activeFunction_ = funcDecl;
    FlowResult flow = AnalyzeStatements(funcDecl->codeBlock->stmnts, env);
    activeFunction_ = previous;

    if (contract)
    {
        std::vector<BindingEnvironment> exits = flow.returns;
        if (flow.fallsThrough)
            exits.push_back(flow.fallthrough);

        for (const auto& param : contract->parameters)
        {
            if (!param.output || !param.variable || !param.layout || !param.layout->HasOpaque())
                continue;

            OpaqueValue joined;
            for (const auto& exit : exits)
            {
                auto it = exit.find(param.variable);
                if (it != exit.end())
                    joined = (joined.Valid() ? JoinValue(joined, it->second) : it->second);
            }

            if (joined.Valid())
                summary.outValues[param.variable] = joined;
        }
    }

    summary.analyzing = false;
    summary.analyzed  = true;
}

FlowResult OpaqueProgramAnalyzer::AnalyzeStatements(const std::vector<StmntPtr>& statements, BindingEnvironment env)
{
    FlowResult result;
    result.fallthrough = env;

    for (const auto& statement : statements)
    {
        if (!result.fallsThrough)
            break;

        FlowResult next = AnalyzeStatement(statement, result.fallthrough);
        result.returns.insert(result.returns.end(), next.returns.begin(), next.returns.end());
        result.breaks.insert(result.breaks.end(), next.breaks.begin(), next.breaks.end());
        result.continues.insert(result.continues.end(), next.continues.begin(), next.continues.end());
        result.discards.insert(result.discards.end(), next.discards.begin(), next.discards.end());
        result.fallsThrough = next.fallsThrough;
        result.fallthrough  = next.fallthrough;
    }

    return result;
}

FlowResult OpaqueProgramAnalyzer::AnalyzeStatement(const StmntPtr& statement, BindingEnvironment env)
{
    FlowResult result;
    result.fallthrough = env;
    if (!statement)
        return result;

    if (auto block = statement->As<CodeBlockStmnt>())
        return AnalyzeStatements(block->codeBlock->stmnts, env);

    if (auto decl = statement->As<VarDeclStmnt>())
    {
        for (const auto& var : decl->varDecls)
        {
            auto layout = layouts_.Get(var.get(), var.get());
            plan_.variableLayouts[var.get()] = layout;

            if (layout && layout->HasOpaque())
            {
                OpaqueValue value = MakeUninitialized(layout);
                if (var->initializer)
                    value = Evaluate(var->initializer, env, layout, layout->root);

                env[var.get()] = value;
            }
            else if (var->initializer)
                Evaluate(var->initializer, env, nullptr, nullptr);
        }

        result.fallthrough = env;
        return result;
    }

    if (auto exprStmnt = statement->As<ExprStmnt>())
    {
        Evaluate(exprStmnt->expr, env, nullptr, nullptr);

        result.fallthrough = env;
        return result;
    }

    if (auto ret = statement->As<ReturnStmnt>())
    {
        if (ret->expr)
        {
            FunctionContract* contract = FindContract(plan_, activeFunction_);
            OpaqueValue value = Evaluate(ret->expr, env, (contract ? contract->returnLayout : nullptr), (contract && contract->returnLayout ? contract->returnLayout->root : nullptr));

            if (value.Valid())
            {
                auto& summary = plan_.summaries[activeFunction_];
                summary.returnValue = (summary.hasReturn ? JoinValue(summary.returnValue, value) : value);
                summary.hasReturn   = true;
            }
        }

        result.fallsThrough = false;
        result.returns.push_back(env);

        return result;
    }

    if (auto transfer = statement->As<CtrlTransferStmnt>())
    {
        result.fallsThrough = false;
        switch (transfer->transfer)
        {
            case CtrlTransfer::Break:    result.breaks.push_back(env);    break;
            case CtrlTransfer::Continue: result.continues.push_back(env); break;
            case CtrlTransfer::Discard:  result.discards.push_back(env);  break;
            default: break;
        }

        return result;
    }

    if (auto ifStmnt = statement->As<IfStmnt>())
    {
        Evaluate(ifStmnt->condition, env, nullptr, nullptr);

        FlowResult thenFlow = AnalyzeStatement(ifStmnt->bodyStmnt, env);
        FlowResult elseFlow;
        if (ifStmnt->elseStmnt)
            elseFlow = AnalyzeStatement(ifStmnt->elseStmnt->bodyStmnt, env);
        else
        {
            elseFlow.fallsThrough = true;
            elseFlow.fallthrough  = env;
        }

        MergeExits(result, thenFlow, elseFlow);
        return result;
    }

    if (auto forLoop = statement->As<ForLoopStmnt>())
    {
        FlowResult init = AnalyzeStatement(forLoop->initStmnt, env);

        BindingEnvironment entry = init.fallthrough;
        Evaluate(forLoop->condition, entry, nullptr, nullptr);

        FlowResult body = AnalyzeStatement(forLoop->bodyStmnt, entry);
        BindingEnvironment backEdge = entry;
        if (body.fallsThrough)
        {
            backEdge = JoinEnvironment(backEdge, body.fallthrough);
            Evaluate(forLoop->iteration, backEdge, nullptr, nullptr);
        }

        for (const auto& continueEnv : body.continues)
        {
            BindingEnvironment iterationEnv = continueEnv;
            Evaluate(forLoop->iteration, iterationEnv, nullptr, nullptr);
            backEdge = JoinEnvironment(backEdge, iterationEnv);
        }

        result.fallthrough = JoinEnvironment(entry, backEdge);
        for (const auto& breakEnv : body.breaks)
            result.fallthrough = JoinEnvironment(result.fallthrough, breakEnv);

        result.returns = body.returns;
        result.discards = body.discards;
        return result;
    }

    if (auto whileLoop = statement->As<WhileLoopStmnt>())
    {
        Evaluate(whileLoop->condition, env, nullptr, nullptr);

        FlowResult body = AnalyzeStatement(whileLoop->bodyStmnt, env);
        result.fallthrough = env;

        if (body.fallsThrough)
            result.fallthrough = JoinEnvironment(result.fallthrough, body.fallthrough);

        for (const auto& continueEnv : body.continues)
            result.fallthrough = JoinEnvironment(result.fallthrough, continueEnv);

        for (const auto& breakEnv : body.breaks)
            result.fallthrough = JoinEnvironment(result.fallthrough, breakEnv);

        result.returns = body.returns;
        result.discards = body.discards;
        return result;
    }

    if (auto doLoop = statement->As<DoWhileLoopStmnt>())
    {
        FlowResult body = AnalyzeStatement(doLoop->bodyStmnt, env);

        result.fallsThrough = body.fallsThrough || !body.breaks.empty();
        if (body.fallsThrough)
        {
            result.fallthrough = body.fallthrough;
            Evaluate(doLoop->condition, result.fallthrough, nullptr, nullptr);
        }
        for (const auto& continueEnv : body.continues)
        {
            BindingEnvironment continued = continueEnv;
            Evaluate(doLoop->condition, continued, nullptr, nullptr);
            result.fallthrough = (result.fallthrough.empty() ? continued : JoinEnvironment(result.fallthrough, continued));
        }
        for (const auto& breakEnv : body.breaks)
            result.fallthrough = (result.fallthrough.empty() ? breakEnv : JoinEnvironment(result.fallthrough, breakEnv));
        result.returns = body.returns;
        result.discards = body.discards;
        return result;
    }
    if (auto switchStmnt = statement->As<SwitchStmnt>())
    {
        Evaluate(switchStmnt->selector, env, nullptr, nullptr);
        FlowResult switchFlow;
        switchFlow.fallsThrough = false;
        BindingEnvironment pendingFallthrough;
        bool hasPendingFallthrough = false;
        bool hasDefault = false;
        for (const auto& switchCase : switchStmnt->cases)
        {
            hasDefault = hasDefault || switchCase->IsDefaultCase();
            if (switchCase->expr)
                Evaluate(switchCase->expr, env, nullptr, nullptr);
            BindingEnvironment caseEntry = (hasPendingFallthrough ? JoinEnvironment(env, pendingFallthrough) : env);
            FlowResult caseFlow = AnalyzeStatements(switchCase->stmnts, caseEntry);
            switchFlow.returns.insert(switchFlow.returns.end(), caseFlow.returns.begin(), caseFlow.returns.end());
            switchFlow.continues.insert(switchFlow.continues.end(), caseFlow.continues.begin(), caseFlow.continues.end());
            switchFlow.discards.insert(switchFlow.discards.end(), caseFlow.discards.begin(), caseFlow.discards.end());
            for (const auto& breakEnv : caseFlow.breaks)
            {
                switchFlow.fallthrough = (switchFlow.fallsThrough ? JoinEnvironment(switchFlow.fallthrough, breakEnv) : breakEnv);
                switchFlow.fallsThrough = true;
            }
            hasPendingFallthrough = caseFlow.fallsThrough;
            if (hasPendingFallthrough)
                pendingFallthrough = caseFlow.fallthrough;
        }
        if (hasPendingFallthrough)
        {
            switchFlow.fallthrough = (switchFlow.fallsThrough ? JoinEnvironment(switchFlow.fallthrough, pendingFallthrough) : pendingFallthrough);
            switchFlow.fallsThrough = true;
        }
        if (!hasDefault)
        {
            switchFlow.fallthrough = (switchFlow.fallsThrough ? JoinEnvironment(switchFlow.fallthrough, env) : env);
            switchFlow.fallsThrough = true;
        }
        return switchFlow;
    }

    return result;
}

void OpaqueProgramAnalyzer::MergeExits(FlowResult& result, const FlowResult& lhs, const FlowResult& rhs)
{
    result.fallsThrough = lhs.fallsThrough || rhs.fallsThrough;

    if (lhs.fallsThrough && rhs.fallsThrough)
        result.fallthrough = JoinEnvironment(lhs.fallthrough, rhs.fallthrough);
    else if (lhs.fallsThrough)
        result.fallthrough = lhs.fallthrough;
    else if (rhs.fallsThrough)
        result.fallthrough = rhs.fallthrough;

    result.returns = lhs.returns;
    result.returns.insert(result.returns.end(), rhs.returns.begin(), rhs.returns.end());
    result.breaks = lhs.breaks;
    result.breaks.insert(result.breaks.end(), rhs.breaks.begin(), rhs.breaks.end());
    result.continues = lhs.continues;
    result.continues.insert(result.continues.end(), rhs.continues.begin(), rhs.continues.end());
    result.discards = lhs.discards;
    result.discards.insert(result.discards.end(), rhs.discards.begin(), rhs.discards.end());
}

OpaqueValue OpaqueProgramAnalyzer::Evaluate(const ExprPtr& expr, BindingEnvironment& env, const OpaqueTypeLayoutPtr& expectedLayout, const LayoutNodePtr& expectedNode)
{
    OpaqueValue none;
    if (!expr)
        return none;

    if (auto assign = expr->As<AssignExpr>())
        return EvaluateAssignment(*assign, env);

    if (auto call = expr->As<CallExpr>())
        return EvaluateCall(*call, env);

    if (auto ternary = expr->As<TernaryExpr>())
    {
        Evaluate(ternary->condExpr, env, nullptr, nullptr);

        BindingEnvironment thenEnv = env;
        BindingEnvironment elseEnv = env;
        OpaqueValue thenValue = Evaluate(ternary->thenExpr, thenEnv, expectedLayout, expectedNode);
        OpaqueValue elseValue = Evaluate(ternary->elseExpr, elseEnv, expectedLayout, expectedNode);
        env = JoinEnvironment(thenEnv, elseEnv);

        OpaqueValue value = JoinValue(thenValue, elseValue);
        if (value.Valid())
            plan_.expressionValues[expr.get()] = value;

        return value;
    }

    if (auto initializer = expr->As<InitializerExpr>())
        return EvaluateInitializer(*initializer, env, expectedLayout, expectedNode);

    if (auto descriptor = expr->As<DescriptorHeapExpr>())
    {
        if (descriptor->resolvedTypeDenoter)
        {
            OpaqueValue value;
            value.layout = expectedLayout;
            value.node   = expectedNode;
            if (!value.node)
            {
                value.layout = layouts_.Get(descriptor->resolvedTypeDenoter, descriptor);
                value.node   = (value.layout ? value.layout->root : nullptr);
            }
            if (value.node && value.node->hasOpaque)
            {
                value.bindings.resize(value.node->laneIndices.size());
                for (auto& binding : value.bindings)
                {
                    binding = OpaqueBinding::Descriptor(
                        descriptor->heap,
                        descriptor->resolvedTypeDenoter,
                        descriptor->index
                    );
                }
                plan_.expressionValues[expr.get()] = value;
                return value;
            }
        }
    }

    OpaqueValue accessValue = EvaluateAccess(expr, env);
    if (accessValue.Valid())
    {
        plan_.expressionValues[expr.get()] = accessValue;
        return accessValue;
    }

    Decl* resource = nullptr;
    std::vector<IndexUse> resourceIndices;
    if (DescribeResource(expr, resource, resourceIndices))
    {
        OpaqueValue value;
        value.layout = expectedLayout;
        value.node   = expectedNode;
        if (!value.node)
        {
            auto resourceType = resource->GetTypeDenoter();
            value.layout = layouts_.Get(resourceType, expr.get());
            value.node   = (value.layout ? value.layout->root : nullptr);
        }

        if (value.node && value.node->hasOpaque)
        {
            value.bindings.resize(value.node->laneIndices.size());
            for (std::size_t i = 0; i < value.bindings.size(); ++i)
            {
                std::vector<IndexUse> indices = resourceIndices;
                if (value.bindings.size() > 1)
                {
                    const std::size_t laneIndex = value.node->laneIndices[i];
                    const AccessPath& path = value.layout->lanes[laneIndex].path;
                    for (const auto& step : path)
                    {
                        if (step.kind == PathStep::Kind::Index)
                        {
                            IndexUse index;
                            index.constant = step.index;
                            indices.push_back(index);
                        }
                    }
                }

                value.bindings[i] = OpaqueBinding::Resource(resource, indices);
            }

            plan_.expressionValues[expr.get()] = value;
            return value;
        }
    }

    if (auto bracket = expr->As<BracketExpr>())
        return Evaluate(bracket->expr, env, expectedLayout, expectedNode);

    if (auto sequence = expr->As<SequenceExpr>())
    {
        OpaqueValue value;
        for (const auto& sub : sequence->exprs)
            value = Evaluate(sub, env, expectedLayout, expectedNode);

        return value;
    }

    if (auto binary = expr->As<BinaryExpr>())
    {
        Evaluate(binary->lhsExpr, env, nullptr, nullptr);
        if (binary->op == BinaryOp::LogicalAnd || binary->op == BinaryOp::LogicalOr)
        {
            BindingEnvironment rhsEnv = env;
            Evaluate(binary->rhsExpr, rhsEnv, nullptr, nullptr);
            env = JoinEnvironment(env, rhsEnv);
        }
        else
            Evaluate(binary->rhsExpr, env, nullptr, nullptr);
    }
    else if (auto unary = expr->As<UnaryExpr>())
        Evaluate(unary->expr, env, nullptr, nullptr);
    else if (auto post = expr->As<PostUnaryExpr>())
        Evaluate(post->expr, env, nullptr, nullptr);
    else if (auto cast = expr->As<CastExpr>())
        Evaluate(cast->expr, env, nullptr, nullptr);

    return none;
}

OpaqueValue OpaqueProgramAnalyzer::EvaluateInitializer( InitializerExpr& initializer, BindingEnvironment& env, const OpaqueTypeLayoutPtr& layout, const LayoutNodePtr& node)
{
    if (!layout || !node || !node->hasOpaque)
    {
        for (const auto& element : initializer.exprs)
            Evaluate(element, env, nullptr, nullptr);

        return OpaqueValue{};
    }

    OpaqueValue value = MakeUninitialized(layout, node);
    std::vector<ExprPtr> elements;
    initializer.CollectElements(elements);
    for (std::size_t i = 0; i < node->laneIndices.size(); ++i)
    {
        const std::size_t laneIndex = node->laneIndices[i];
        const std::size_t source = layout->lanes[laneIndex].sourceOrdinal;
        if (source < elements.size())
        {
            OpaqueValue leaf = Evaluate(elements[source], env, layout, FindLaneNode(layout->root, laneIndex));
            if (!leaf.bindings.empty())
                value.bindings[i] = leaf.bindings.front();
        }
    }

    plan_.expressionValues[&initializer] = value;
    return value;
}

LayoutNodePtr OpaqueProgramAnalyzer::FindLaneNode(const LayoutNodePtr& node, std::size_t laneIndex)
{
    if (!node)
        return nullptr;

    if (node->kind == LayoutNode::Kind::Opaque && !node->laneIndices.empty() && node->laneIndices.front() == laneIndex)
        return node;

    for (const auto& child : node->children)
    {
        auto found = FindLaneNode(child.node, laneIndex);
        if (found)
            return found;
    }

    return nullptr;
}

OpaqueValue OpaqueProgramAnalyzer::EvaluateAccess(const ExprPtr& expr, BindingEnvironment& env)
{
    // Treat buffer[index].member as a data read, not a projection of the resource alias itself.
    if (auto object = (expr ? expr->As<ObjectExpr>() : nullptr))
    {
        if (auto element = (object->prefixExpr ? object->prefixExpr->As<ArrayExpr>() : nullptr))
        {
            const auto resourceType = element->prefixExpr->GetTypeDenoter()->GetSub();
            if (resourceType->GetAliased().As<BufferTypeDenoter>())
            {
                Evaluate(object->prefixExpr, env, nullptr, nullptr);
                return OpaqueValue{};
            }
        }
    }

    if (auto array = (expr ? expr->As<ArrayExpr>() : nullptr))
    {
        // Treat buffer[index] as a data read while still resolving the resource alias in its prefix.
        const auto prefixType = array->prefixExpr->GetTypeDenoter()->GetSub();
        if (prefixType->GetAliased().As<BufferTypeDenoter>())
        {
            Evaluate(array->prefixExpr, env, nullptr, nullptr);
            for (const auto& index : array->arrayIndices)
                Evaluate(index, env, nullptr, nullptr);
            return OpaqueValue{};
        }
    }

    AccessDescription access;
    if (DescribeAccess(expr, access))
    {
        auto it = env.find(access.root);
        if (it != env.end())
        {
            OpaqueValue projected = Project(it->second, access, expr.get());
            if (projected.Valid())
                return projected;
        }
    }

    if (auto object = expr->As<ObjectExpr>())
    {
        if (object->prefixExpr && object->FetchVarDecl())
        {
            OpaqueValue prefix = Evaluate(object->prefixExpr, env, nullptr, nullptr);
            if (prefix.Valid())
                return ProjectRelative(prefix, PathStep::Field(object->FetchVarDecl()), expr.get());
        }
    }
    else if (auto array = expr->As<ArrayExpr>())
    {
        OpaqueValue prefix = Evaluate(array->prefixExpr, env, nullptr, nullptr);
        for (const auto& indexExpr : array->arrayIndices)
        {
            if (!prefix.Valid())
                break;

            int index = 0;
            if (ParseConstantIndex(indexExpr, index))
                prefix = ProjectRelative(prefix, PathStep::Index(index), expr.get());
            else
                prefix = ProjectDynamic(prefix, indexExpr, expr.get());
        }

        return prefix;
    }

    return OpaqueValue{};
}

OpaqueValue OpaqueProgramAnalyzer::Project(const OpaqueValue& rootValue, const AccessDescription& access, const AST* context)
{
    OpaqueValue value = rootValue;
    for (std::size_t pos = 0; pos < access.constantPath.size(); ++pos)
    {
        auto dynamicStep = std::find_if(access.dynamicSteps.begin(), access.dynamicSteps.end(),
            [&](const DynamicStep& step) { return step.position == pos; }
        );

        if (dynamicStep != access.dynamicSteps.end())
            value = ProjectDynamic(value, dynamicStep->expression, context);
        else
            value = ProjectRelative(value, access.constantPath[pos], context);

        if (!value.Valid())
            break;
    }

    return value;
}

OpaqueValue OpaqueProgramAnalyzer::ProjectRelative(const OpaqueValue& value, const PathStep& step, const AST*)
{
    if (!value.Valid())
        return OpaqueValue{};

    auto it = std::find_if(value.node->children.begin(), value.node->children.end(),
        [&](const LayoutNode::Child& child) { return SameStep(child.step, step); }
    );

    if (it == value.node->children.end() || !it->node->hasOpaque)
        return OpaqueValue{};

    OpaqueValue projected;
    projected.layout = value.layout;
    projected.node   = it->node;

    for (const auto laneIndex : it->node->laneIndices)
    {
        auto parentIt = std::find(value.node->laneIndices.begin(), value.node->laneIndices.end(), laneIndex);
        if (parentIt != value.node->laneIndices.end())
            projected.bindings.push_back(value.bindings[static_cast<std::size_t>(parentIt - value.node->laneIndices.begin())]);
    }

    return projected;
}

OpaqueValue OpaqueProgramAnalyzer::ProjectDynamic(const OpaqueValue& value, const ExprPtr& indexExpr, const AST* context)
{
    if (!value.Valid() || value.node->kind != LayoutNode::Kind::Array || value.node->children.empty())
        RuntimeErr(R_OpaqueTypeInvalidRuntimeIndex("non-array value"), context);

    /* A fixed-size array layout has one child per constant element. A runtime
       index may select any of them, so enumerate those alternatives first.
       ProjectRelative is the common operation that selects a child node and
       slices value.bindings to just the opaque lanes below that child. */
    std::vector<OpaqueValue> elementCandidates;
    elementCandidates.reserve(value.node->children.size());
    for (const auto& fixedElement : value.node->children)
        elementCandidates.push_back(ProjectRelative(value, fixedElement.step, context));
    if (elementCandidates.empty() || !elementCandidates.front().Valid())
        return OpaqueValue{};

    /* Every candidate has the same element shape. Use the first candidate as
       that shape's exemplar, then replace each of its fixed bindings with the
       single dynamic binding that represents the same relative lane across
       every possible element. */
    OpaqueValue dynamicElement = elementCandidates.front();
    for (std::size_t relativeLaneIndex = 0; relativeLaneIndex < dynamicElement.bindings.size(); ++relativeLaneIndex)
    {
        dynamicElement.bindings[relativeLaneIndex] =
            GeneralizeDynamicLane(elementCandidates, relativeLaneIndex, indexExpr, context);
    }
    return dynamicElement;
}

OpaqueBinding OpaqueProgramAnalyzer::GeneralizeDynamicLane(
    const std::vector<OpaqueValue>& elementCandidates,
    std::size_t relativeLaneIndex,
    const ExprPtr& indexExpr,
    const AST* context)
{
    bool cohesive = true;
    OpaqueBinding generalizedBinding = elementCandidates.front().bindings[relativeLaneIndex];
    if (generalizedBinding.kind == OpaqueBinding::Kind::Resource)
    {
        /* Accept resources[0], resources[1], ... (with an identical prefix
           for multidimensional arrays) and generalize the varying final axis
           to resources[indexExpr]. */
        if (generalizedBinding.indices.empty() ||
            generalizedBinding.indices.back().dynamic ||
            generalizedBinding.indices.back().constant != 0)
            cohesive = false;
        else
        {
            for (std::size_t elementIndex = 1; elementIndex < elementCandidates.size(); ++elementIndex)
            {
                const auto& elementBinding = elementCandidates[elementIndex].bindings[relativeLaneIndex];
                if (elementBinding.kind != OpaqueBinding::Kind::Resource ||
                    elementBinding.resource != generalizedBinding.resource ||
                    elementBinding.indices.size() != generalizedBinding.indices.size())
                {
                    cohesive = false;
                    break;
                }
                for (std::size_t axis = 0; axis + 1 < generalizedBinding.indices.size(); ++axis)
                {
                    const auto& expectedIndex = generalizedBinding.indices[axis];
                    const auto& elementIndexUse = elementBinding.indices[axis];
                    if (expectedIndex.dynamic != elementIndexUse.dynamic ||
                        (expectedIndex.dynamic ?
                            expectedIndex.expression.get() != elementIndexUse.expression.get() :
                            expectedIndex.constant != elementIndexUse.constant))
                        cohesive = false;
                }
                if (elementBinding.indices.back().dynamic ||
                    elementBinding.indices.back().constant != static_cast<int>(elementIndex))
                    cohesive = false;
            }
        }
        if (cohesive)
        {
            generalizedBinding.indices.back().dynamic    = true;
            generalizedBinding.indices.back().expression = indexExpr;
        }
    }
    else if (generalizedBinding.kind == OpaqueBinding::Kind::FormalLane)
    {
        /* Formal lanes do not name concrete resources yet. Record which lane
           from each fixed element forms this runtime-indexed axis; BuildABIs
           will expose that group as one opaque-array input parameter. */
        std::vector<std::size_t> laneGroup;
        laneGroup.push_back(generalizedBinding.laneIndex);
        for (std::size_t elementIndex = 1; elementIndex < elementCandidates.size(); ++elementIndex)
        {
            const auto& elementBinding = elementCandidates[elementIndex].bindings[relativeLaneIndex];
            if (elementBinding.kind != OpaqueBinding::Kind::FormalLane ||
                elementBinding.formal != generalizedBinding.formal)
                cohesive = false;
            else
                laneGroup.push_back(elementBinding.laneIndex);
        }
        if (cohesive)
        {
            auto& groups = plan_.dynamicLaneGroups[generalizedBinding.formal];
            if (std::find(groups.begin(), groups.end(), laneGroup) == groups.end())
                groups.push_back(laneGroup);
            generalizedBinding.kind = OpaqueBinding::Kind::FormalArrayLane;
            IndexUse index;
            index.dynamic    = true;
            index.expression = indexExpr;
            generalizedBinding.indices.push_back(index);
        }
    }
    else
        cohesive = false;

    if (!cohesive)
        RuntimeErr(R_OpaqueTypeInvalidRuntimeIndex("unrelated resource bindings"), context);
    return generalizedBinding;
}

OpaqueValue OpaqueProgramAnalyzer::EvaluateAssignment(AssignExpr& assign, BindingEnvironment& env)
{
    // Treat buffer[index] assignments as data writes, not assignments to the resource alias itself.
    if (IsResourceDataAccess(assign.lvalueExpr))
    {
        Evaluate(assign.lvalueExpr, env, nullptr, nullptr);
        return Evaluate(assign.rvalueExpr, env, nullptr, nullptr);
    }

    AccessDescription access;
    if (DescribeAccess(assign.lvalueExpr, access))
    {
        auto it = env.find(access.root);
        if (it != env.end())
        {
            if (!access.dynamicSteps.empty())
                RuntimeErr(R_OpaqueTypeDynamicWrite(access.root->ident.Original()), &assign);

            OpaqueValue target = Project(it->second, access, &assign);
            if (target.Valid())
            {
                OpaqueValue source = Evaluate(assign.rvalueExpr, env, target.layout, target.node);
                AssignProjected(it->second, target.node, source);

                plan_.opaqueOnlyAssignments[&assign] = !target.node->hasResidual;
                return target;
            }
        }
    }

    Evaluate(assign.lvalueExpr, env, nullptr, nullptr);
    return Evaluate(assign.rvalueExpr, env, nullptr, nullptr);
}

void OpaqueProgramAnalyzer::AssignProjected(OpaqueValue& destination, const LayoutNodePtr& targetNode, const OpaqueValue& source)
{
    if (!targetNode || !source.Valid())
        return;

    for (std::size_t i = 0; i < targetNode->laneIndices.size() && i < source.bindings.size(); ++i)
    {
        const auto laneIndex = targetNode->laneIndices[i];
        auto it = std::find(destination.node->laneIndices.begin(), destination.node->laneIndices.end(), laneIndex);
        if (it != destination.node->laneIndices.end())
            destination.bindings[static_cast<std::size_t>(it - destination.node->laneIndices.begin())] = source.bindings[i];
    }
}

OpaqueValue OpaqueProgramAnalyzer::EvaluateCall(CallExpr& call, BindingEnvironment& env)
{
    if (call.prefixExpr)
        Evaluate(call.prefixExpr, env, nullptr, nullptr);

    FunctionDecl* callee = call.GetFunctionImpl();
    if (!callee)
        callee = call.GetFunctionDecl();

    callee = CanonicalFunction(callee);

    FunctionContract* contract = FindContract(plan_, callee);
    if (!contract)
    {
        for (const auto& arg : call.arguments)
            Evaluate(arg, env, nullptr, nullptr);

        return OpaqueValue{};
    }

    AnalyzeFunction(callee);

    CallSitePlan callPlan;
    callPlan.contract = contract;
    callPlan.arguments.resize(contract->parameters.size());

    for (std::size_t i = 0; i < contract->parameters.size(); ++i)
    {
        const auto& param = contract->parameters[i];
        if (i >= call.arguments.size())
        {
            if (param.layout && param.layout->HasOpaque())
                RuntimeErr(R_OpaqueTypeDefaultArgument(param.variable ? param.variable->ident.Original() : std::string("parameter")), &call);

            continue;
        }

        callPlan.arguments[i] = Evaluate(call.arguments[i], env, param.layout, (param.layout ? param.layout->root : nullptr));
    }

    auto summaryIt = plan_.summaries.find(callee);
    if (summaryIt != plan_.summaries.end())
    {
        const auto& summary = summaryIt->second;
        if (summary.hasReturn)
            callPlan.result = Remap(summary.returnValue, *contract, callPlan.arguments);

        for (std::size_t i = 0; i < contract->parameters.size() && i < call.arguments.size(); ++i)
        {
            const auto& param = contract->parameters[i];
            if (!param.output || !param.variable)
                continue;

            auto outIt = summary.outValues.find(param.variable);
            if (outIt == summary.outValues.end())
                continue;

            OpaqueValue translated = Remap(outIt->second, *contract, callPlan.arguments);
            AccessDescription destination;
            if (DescribeAccess(call.arguments[i], destination))
            {
                auto envIt = env.find(destination.root);
                if (envIt != env.end())
                {
                    OpaqueValue target = Project(envIt->second, destination, &call);
                    AssignProjected(envIt->second, target.node, translated);
                }
            }
        }
    }

    plan_.callSites[&call] = callPlan;

    if (callPlan.result.Valid())
        plan_.expressionValues[&call] = callPlan.result;

    return callPlan.result;
}

OpaqueValue OpaqueProgramAnalyzer::Remap(const OpaqueValue& value, const FunctionContract& contract, const std::vector<OpaqueValue>& arguments)
{
    OpaqueValue result = value;
    for (auto& binding : result.bindings)
    {
        if (binding.kind != OpaqueBinding::Kind::FormalLane && binding.kind != OpaqueBinding::Kind::FormalArrayLane)
            continue;

        for (std::size_t p = 0; p < contract.parameters.size(); ++p)
        {
            if (contract.parameters[p].variable != binding.formal || p >= arguments.size())
                continue;

            const auto& arg = arguments[p];
            if (binding.laneIndex < arg.bindings.size())
                binding = arg.bindings[binding.laneIndex];

            break;
        }
    }
    return result;
}


} // /namespace OpaqueLowering

} // /namespace Xsc



// ================================================================================
