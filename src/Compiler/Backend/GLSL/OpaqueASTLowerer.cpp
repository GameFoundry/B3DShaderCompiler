/*
 * OpaqueASTLowerer.cpp
 */

#include "OpaqueASTLowerer.h"
#include "AST.h"
#include "ASTFactory.h"
#include "Converter.h"
#include "Exception.h"
#include "ReportIdents.h"
#include "TypeDenoter.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>


namespace Xsc
{

namespace OpaqueLowering
{


OpaqueASTLowerer::OpaqueASTLowerer(Program& program, OpaqueTypeLayoutCache& layouts, const OpaqueProgramPlan& plan) :
    program_ { program }, layouts_ { layouts }, plan_ { plan }
{
}

void OpaqueASTLowerer::Lower()
{
    RewriteSignatures();
    RewriteBodies();
    StripTypeDeclarations();
}


/* ----- Signature rewriting ----- */

void OpaqueASTLowerer::RewriteSignatures()
{
    for (std::size_t contractIndex = 0; contractIndex < plan_.contracts.size(); ++contractIndex)
    {
        auto& contract = plan_.contracts[contractIndex];
        auto& abi      = plan_.abis[contractIndex];
        auto& synthesized = synthesizedSignatures_[contract.declaration];
        std::vector<VarDeclStmntPtr> parameters;

        for (std::size_t p = 0; p < contract.parameters.size(); ++p)
        {
            const auto& parameter = contract.parameters[p];
            const auto& parameterABI = abi.parameters[p];
            if (parameterABI.keepResidual)
                parameters.push_back(parameter.declaration);

            if (!parameter.layout || !parameter.layout->HasOpaque() || parameterABI.nativeOpaque || !parameter.input)
                continue;

            auto& laneParams = synthesized.laneParameters[parameter.variable];
            laneParams.resize(parameter.layout->lanes.size(), nullptr);
            const std::string baseName =
                (parameter.variable != nullptr ? parameter.variable->ident.Original() : std::string("opaque"));
            for (const auto laneIndex : parameterABI.scalarLanes)
            {
                const auto& lane = parameter.layout->lanes[laneIndex];
                const std::string suffix = PathName(lane.path);
                auto laneParam = ASTFactory::MakeVarDeclStmnt(
                    ASTFactory::MakeTypeSpecifier(lane.type->Copy()),
                    baseName + "_opaque_" + (suffix.empty() ? std::to_string(laneIndex) : suffix)
                );
                laneParam->flags << VarDeclStmnt::isParameter;
                laneParam->typeSpecifier->isInput  = true;
                laneParam->typeSpecifier->isOutput = false;
                if (!laneParam->varDecls.empty())
                {
                    laneParams[laneIndex] = laneParam->varDecls.front().get();
                    laneParam->varDecls.front()->initializer.reset();
                }
                parameters.push_back(laneParam);
            }
            for (const auto& group : parameterABI.arrayLaneGroups)
            {
                if (group.empty())
                    continue;
                const std::size_t firstLane = group.front();
                const auto& lane = parameter.layout->lanes[firstLane];
                auto laneParam = ASTFactory::MakeVarDeclStmnt(
                    ASTFactory::MakeTypeSpecifier(lane.type->Copy()),
                    baseName + "_opaque_array_" + PathName(lane.path)
                );
                laneParam->flags << VarDeclStmnt::isParameter;
                laneParam->typeSpecifier->isInput   = true;
                laneParam->typeSpecifier->isOutput  = false;
                if (!laneParam->varDecls.empty())
                {
                    VarDecl* arrayParam = laneParam->varDecls.front().get();
                    arrayParam->arrayDims = ASTFactory::MakeArrayDimensionList(
                        std::vector<int>(1, static_cast<int>(group.size()))
                    );
                    for (const auto laneIndex : group)
                    {
                        if (laneIndex < laneParams.size())
                            laneParams[laneIndex] = arrayParam;
                    }
                }
                parameters.push_back(laneParam);
            }
        }

        if (abi.lowerReturn)
        {
            contract.declaration->returnType = ASTFactory::MakeTypeSpecifier(std::make_shared<VoidTypeDenoter>());
            contract.declaration->semantic  = Semantic::Undefined;
            if (abi.returnResidual)
            {
                auto outParam = ASTFactory::MakeVarDeclStmnt(
                    ASTFactory::MakeTypeSpecifier(contract.originalReturnType->typeDenoter->Copy()),
                    contract.declaration->ident.Original() + "_opaque_result"
                );
                outParam->flags << VarDeclStmnt::isParameter;
                outParam->typeSpecifier->isInput  = false;
                outParam->typeSpecifier->isOutput = true;
                if (!outParam->varDecls.empty())
                {
                    synthesized.returnResidual = outParam->varDecls.front().get();
                    outParam->varDecls.front()->semantic = contract.declaration->semantic;
                }
                parameters.push_back(outParam);
            }
        }
        contract.declaration->parameters = std::move(parameters);
    }
}


/* ----- Statement lowering ----- */

void OpaqueASTLowerer::RewriteBodies()
{
    ForEachFunction(program_, [&](FunctionDecl* function)
    {
        if (!function->codeBlock)
            return;
        activeFunction_ = function;
        LowerStatementList(function->codeBlock->stmnts);
        activeFunction_ = nullptr;
    });
}

void OpaqueASTLowerer::LowerStatementList(std::vector<StmntPtr>& statements)
{
    std::vector<StmntPtr> lowered;
    for (auto& statement : statements)
    {
        auto replacement = LowerStatement(statement);
        lowered.insert(lowered.end(), replacement.begin(), replacement.end());
    }
    statements = std::move(lowered);
}

std::vector<StmntPtr> OpaqueASTLowerer::LowerStatement(const StmntPtr& statement)
{
    if (!statement)
        return std::vector<StmntPtr>();

    if (auto block = statement->As<CodeBlockStmnt>())
    {
        LowerStatementList(block->codeBlock->stmnts);
        return SingleStatement(statement);
    }
    if (auto decl = statement->As<VarDeclStmnt>())
        return LowerVariableDeclaration(statement, *decl);
    if (auto exprStmnt = statement->As<ExprStmnt>())
        return LowerExpressionStatement(statement, *exprStmnt);
    if (auto ret = statement->As<ReturnStmnt>())
        return LowerReturn(statement, *ret);
    if (auto ifStmnt = statement->As<IfStmnt>())
    {
        LoweredExpression condition = LowerExpression(ifStmnt->condition, nullptr, nullptr);
        ifStmnt->condition = condition.residual;
        ifStmnt->bodyStmnt = LowerEmbedded(ifStmnt->bodyStmnt);
        if (ifStmnt->elseStmnt)
            ifStmnt->elseStmnt->bodyStmnt = LowerEmbedded(ifStmnt->elseStmnt->bodyStmnt);
        condition.prelude.push_back(statement);
        return condition.prelude;
    }
    if (auto forLoop = statement->As<ForLoopStmnt>())
    {
        forLoop->initStmnt = LowerEmbedded(forLoop->initStmnt);
        LoweredExpression condition = LowerExpression(forLoop->condition, nullptr, nullptr);
        LoweredExpression iteration = LowerExpression(forLoop->iteration, nullptr, nullptr);
        /* Loop-condition/iteration preludes cannot be hoisted without changing
           their execution frequency. Calls returning opaque composites there
           are rejected by the analysis before reaching this point in normal use. */
        if (!condition.prelude.empty() || !iteration.prelude.empty())
            RuntimeErr(R_OpaqueTypeInvalidRuntimeIndex("opaque call in loop control"), forLoop);
        forLoop->condition = condition.residual;
        forLoop->iteration = iteration.residual;
        forLoop->bodyStmnt = LowerEmbedded(forLoop->bodyStmnt);
        return SingleStatement(statement);
    }
    if (auto whileLoop = statement->As<WhileLoopStmnt>())
    {
        LoweredExpression condition = LowerExpression(whileLoop->condition, nullptr, nullptr);
        if (!condition.prelude.empty())
            RuntimeErr(R_OpaqueTypeInvalidRuntimeIndex("opaque call in loop condition"), whileLoop);
        whileLoop->condition = condition.residual;
        whileLoop->bodyStmnt = LowerEmbedded(whileLoop->bodyStmnt);
        return SingleStatement(statement);
    }
    if (auto doLoop = statement->As<DoWhileLoopStmnt>())
    {
        doLoop->bodyStmnt = LowerEmbedded(doLoop->bodyStmnt);
        LoweredExpression condition = LowerExpression(doLoop->condition, nullptr, nullptr);
        if (!condition.prelude.empty())
            RuntimeErr(R_OpaqueTypeInvalidRuntimeIndex("opaque call in loop condition"), doLoop);
        doLoop->condition = condition.residual;
        return SingleStatement(statement);
    }
    if (auto switchStmnt = statement->As<SwitchStmnt>())
    {
        LoweredExpression selector = LowerExpression(switchStmnt->selector, nullptr, nullptr);
        switchStmnt->selector = selector.residual;
        for (auto& switchCase : switchStmnt->cases)
        {
            if (switchCase->expr)
            {
                LoweredExpression caseExpr = LowerExpression(switchCase->expr, nullptr, nullptr);
                if (!caseExpr.prelude.empty())
                    RuntimeErr(R_OpaqueTypeInvalidRuntimeIndex("opaque call in case label"), switchCase.get());
                switchCase->expr = caseExpr.residual;
            }
            LowerStatementList(switchCase->stmnts);
        }
        selector.prelude.push_back(statement);
        return selector.prelude;
    }
    return SingleStatement(statement);
}

std::vector<StmntPtr> OpaqueASTLowerer::LowerVariableDeclaration(const StmntPtr& statement, VarDeclStmnt& declaration)
{
    std::vector<StmntPtr> result;
    bool keepDeclaration = false;
    for (auto& variable : declaration.varDecls)
    {
        auto layoutIt = plan_.variableLayouts.find(variable.get());
        OpaqueTypeLayoutPtr layout =
            (layoutIt != plan_.variableLayouts.end() ? layoutIt->second : layouts_.Get(variable.get(), variable.get()));
        if (!layout || !layout->HasOpaque())
        {
            if (variable->initializer)
            {
                LoweredExpression init = LowerExpression(variable->initializer, nullptr, nullptr);
                result.insert(result.end(), init.prelude.begin(), init.prelude.end());
                variable->initializer = init.residual;
            }
            keepDeclaration = true;
            continue;
        }

        LoweredExpression init;
        if (variable->initializer)
            init = LowerExpression(variable->initializer, layout, layout->root);
        result.insert(result.end(), init.prelude.begin(), init.prelude.end());
        if (layout->HasResidual())
        {
            variable->initializer = init.residual;
            keepDeclaration = true;
        }
        else
            variable->initializer.reset();
    }
    if (keepDeclaration)
        result.push_back(statement);
    return result;
}

std::vector<StmntPtr> OpaqueASTLowerer::LowerExpressionStatement(const StmntPtr& statement, ExprStmnt& exprStmnt)
{
    if (auto assign = (exprStmnt.expr ? exprStmnt.expr->As<AssignExpr>() : nullptr))
    {
        auto opaqueIt = plan_.opaqueOnlyAssignments.find(assign);
        if (opaqueIt != plan_.opaqueOnlyAssignments.end() && opaqueIt->second)
        {
            LoweredExpression rhs = LowerExpression(assign->rvalueExpr, nullptr, nullptr);
            return rhs.prelude;
        }
    }
    LoweredExpression expression = LowerExpression(exprStmnt.expr, nullptr, nullptr);
    if (expression.residual)
    {
        exprStmnt.expr = expression.residual;
        expression.prelude.push_back(statement);
    }
    return expression.prelude;
}

std::vector<StmntPtr> OpaqueASTLowerer::LowerReturn(const StmntPtr& statement, ReturnStmnt& ret)
{
    const FunctionContract* contract = FindContract(plan_, activeFunction_);
    const FunctionABI* abi = FindABI(plan_, activeFunction_);
    if (!contract || !abi || !abi->lowerReturn)
    {
        if (ret.expr)
        {
            LoweredExpression value = LowerExpression(ret.expr, nullptr, nullptr);
            ret.expr = value.residual;
            value.prelude.push_back(statement);
            return value.prelude;
        }
        return SingleStatement(statement);
    }

    LoweredExpression value = LowerExpression(ret.expr, contract->returnLayout, contract->returnLayout->root);
    if (abi->returnResidual)
    {
        VarDecl* outVar = synthesizedSignatures_[activeFunction_].returnResidual;
        if (outVar && value.residual)
            value.prelude.push_back(ASTFactory::MakeAssignStmnt(ASTFactory::MakeObjectExpr(outVar), value.residual));
    }
    ret.expr.reset();
    value.prelude.push_back(statement);
    return value.prelude;
}

StmntPtr OpaqueASTLowerer::LowerEmbedded(const StmntPtr& statement)
{
    auto lowered = LowerStatement(statement);
    if (lowered.empty())
        return std::make_shared<NullStmnt>(SourcePosition::ignore);
    if (lowered.size() == 1)
        return lowered.front();
    auto block = std::make_shared<CodeBlockStmnt>(SourcePosition::ignore);
    block->codeBlock = std::make_shared<CodeBlock>(SourcePosition::ignore);
    block->codeBlock->stmnts = std::move(lowered);
    return block;
}


/* ----- Expression lowering ----- */

LoweredExpression OpaqueASTLowerer::LowerExpression(
    const ExprPtr& expression,
    const OpaqueTypeLayoutPtr& expectedLayout,
    const LayoutNodePtr& expectedNode)
{
    LoweredExpression result;
    if (!expression)
        return result;

    if (auto call = expression->As<CallExpr>())
    {
        auto callIt = plan_.callSites.find(call);
        if (callIt != plan_.callSites.end())
            return LowerCall(expression, *call, callIt->second);

        if (call->prefixExpr)
        {
            LoweredExpression prefix = LowerExpression(call->prefixExpr, nullptr, nullptr);
            Append(result.prelude, prefix.prelude);
            call->prefixExpr = prefix.residual;
        }
        std::vector<ExprPtr> ordinaryArguments;
        std::vector<PendingArgument> pendingArguments;
        for (auto& argument : call->arguments)
        {
            TypeDenoterPtr argumentType = argument->GetTypeDenoter();
            LoweredExpression loweredArg = LowerExpression(argument, nullptr, nullptr);
            if (!loweredArg.prelude.empty())
                FlushPendingArguments(ordinaryArguments, pendingArguments, result.prelude);
            Append(result.prelude, loweredArg.prelude);
            if (loweredArg.residual)
            {
                ordinaryArguments.push_back(loweredArg.residual);
                AddPendingArgument(pendingArguments, ordinaryArguments.size() - 1, argumentType);
            }
        }
        call->arguments = std::move(ordinaryArguments);
        result.residual = expression;
        return ReplaceOpaqueValue(expression, result);
    }
    if (auto initializer = expression->As<InitializerExpr>())
        return LowerInitializer(expression, *initializer, expectedLayout, expectedNode);
    if (auto ternary = expression->As<TernaryExpr>())
        return LowerTernary(expression, *ternary, expectedLayout, expectedNode);
    if (auto binary = expression->As<BinaryExpr>())
        return LowerBinary(expression, *binary);
    if (auto assign = expression->As<AssignExpr>())
    {
        LoweredExpression lhs = LowerExpression(assign->lvalueExpr, nullptr, nullptr);
        LoweredExpression rhs = LowerExpression(assign->rvalueExpr, nullptr, nullptr);
        Append(result.prelude, lhs.prelude);
        Append(result.prelude, rhs.prelude);
        auto opaqueIt = plan_.opaqueOnlyAssignments.find(assign);
        if (opaqueIt != plan_.opaqueOnlyAssignments.end() && opaqueIt->second)
            return result;
        assign->lvalueExpr = lhs.residual;
        assign->rvalueExpr = rhs.residual;
        result.residual = expression;
        return result;
    }
    if (auto object = expression->As<ObjectExpr>())
    {
        if (object->prefixExpr)
        {
            LoweredExpression prefix = LowerExpression(object->prefixExpr, nullptr, nullptr);
            Append(result.prelude, prefix.prelude);
            object->prefixExpr = prefix.residual;
        }
        result.residual = expression;
        return ReplaceOpaqueValue(expression, result);
    }
    if (auto array = expression->As<ArrayExpr>())
    {
        LoweredExpression prefix = LowerExpression(array->prefixExpr, nullptr, nullptr);
        Append(result.prelude, prefix.prelude);
        array->prefixExpr = prefix.residual;
        for (auto& index : array->arrayIndices)
        {
            LoweredExpression loweredIndex = LowerExpression(index, nullptr, nullptr);
            Append(result.prelude, loweredIndex.prelude);
            index = loweredIndex.residual;
        }
        result.residual = expression;
        return ReplaceOpaqueValue(expression, result);
    }
    if (auto bracket = expression->As<BracketExpr>())
    {
        LoweredExpression inner = LowerExpression(bracket->expr, expectedLayout, expectedNode);
        Append(result.prelude, inner.prelude);
        if (inner.residual)
        {
            bracket->expr = inner.residual;
            result.residual = expression;
        }
        return ReplaceOpaqueValue(expression, result);
    }
    if (auto sequence = expression->As<SequenceExpr>())
    {
        std::vector<ExprPtr> residuals;
        for (auto& sub : sequence->exprs)
        {
            LoweredExpression lowered = LowerExpression(sub, nullptr, nullptr);
            Append(result.prelude, lowered.prelude);
            if (lowered.residual)
                residuals.push_back(lowered.residual);
        }
        sequence->exprs = std::move(residuals);
        if (!sequence->exprs.empty())
            result.residual = (sequence->exprs.size() == 1 ? sequence->exprs.front() : expression);
        return ReplaceOpaqueValue(expression, result);
    }
    if (auto unary = expression->As<UnaryExpr>())
    {
        LoweredExpression operand = LowerExpression(unary->expr, nullptr, nullptr);
        Append(result.prelude, operand.prelude);
        unary->expr = operand.residual;
        result.residual = expression;
        return result;
    }
    if (auto post = expression->As<PostUnaryExpr>())
    {
        LoweredExpression operand = LowerExpression(post->expr, nullptr, nullptr);
        Append(result.prelude, operand.prelude);
        post->expr = operand.residual;
        result.residual = expression;
        return result;
    }
    if (auto cast = expression->As<CastExpr>())
    {
        LoweredExpression operand = LowerExpression(cast->expr, nullptr, nullptr);
        Append(result.prelude, operand.prelude);
        cast->expr = operand.residual;
        result.residual = expression;
        return result;
    }

    result.residual = expression;
    return ReplaceOpaqueValue(expression, result);
}

LoweredExpression OpaqueASTLowerer::LowerInitializer(
    const ExprPtr& expression,
    InitializerExpr& initializer,
    const OpaqueTypeLayoutPtr& layout,
    const LayoutNodePtr& node)
{
    LoweredExpression result;
    if (!layout || !node || !node->hasOpaque)
    {
        for (auto& element : initializer.exprs)
        {
            LoweredExpression lowered = LowerExpression(element, nullptr, nullptr);
            Append(result.prelude, lowered.prelude);
            element = lowered.residual;
        }
        result.residual = expression;
        return result;
    }

    std::vector<ExprPtr> elements;
    initializer.CollectElements(elements);
    ExprPtr rebuilt = BuildResidualInitializer(node, elements, node->sourceBegin, result.prelude);
    if (node->hasResidual)
        result.residual = rebuilt;
    return result;
}

ExprPtr OpaqueASTLowerer::BuildResidualInitializer(
    const LayoutNodePtr& node,
    const std::vector<ExprPtr>& elements,
    std::size_t sourceBase,
    std::vector<StmntPtr>& prelude)
{
    if (!node)
        return nullptr;
    if (node->kind == LayoutNode::Kind::Pod || node->kind == LayoutNode::Kind::Opaque)
    {
        const std::size_t localIndex = node->sourceBegin - sourceBase;
        if (localIndex >= elements.size())
            return nullptr;
        LoweredExpression lowered = LowerExpression(elements[localIndex], nullptr, nullptr);
        Append(prelude, lowered.prelude);
        return (node->kind == LayoutNode::Kind::Pod ? lowered.residual : ExprPtr{});
    }

    std::vector<ExprPtr> residualChildren;
    std::vector<PendingArgument> pendingChildren;
    for (const auto& child : node->children)
    {
        std::vector<StmntPtr> childPrelude;
        ExprPtr residual = BuildResidualInitializer(child.node, elements, sourceBase, childPrelude);
        if (!childPrelude.empty())
            FlushPendingArguments(residualChildren, pendingChildren, prelude);
        Append(prelude, childPrelude);
        if (child.node->hasResidual && residual)
        {
            residualChildren.push_back(residual);
            AddPendingArgument(pendingChildren, residualChildren.size() - 1, child.node->sourceType);
        }
    }
    if (!node->hasResidual)
        return nullptr;
    auto initializer = ASTFactory::MakeInitializerExpr(residualChildren);
    /* InitializerExpr has no explicit type field. Cache the declaration-backed
       expected type now so later converters/generators retain nested aggregate
       boundaries instead of inferring the first surviving POD leaf. */
    initializer->GetTypeDenoter(node->sourceType.get());
    return initializer;
}

LoweredExpression OpaqueASTLowerer::LowerCall(const ExprPtr& expression, CallExpr& call, const CallSitePlan& callPlan)
{
    LoweredExpression result;
    if (call.prefixExpr)
    {
        LoweredExpression prefix = LowerExpression(call.prefixExpr, nullptr, nullptr);
        Append(result.prelude, prefix.prelude);
        call.prefixExpr = prefix.residual;
    }

    std::vector<ExprPtr> newArguments;
    std::vector<PendingArgument> pendingArguments;
    const FunctionContract& contract = *callPlan.contract;
    const FunctionABI* abi = FindABI(plan_, contract.canonical);
    for (std::size_t i = 0; i < contract.parameters.size() && i < call.arguments.size(); ++i)
    {
        const auto& parameter = contract.parameters[i];
        const auto& paramABI = abi->parameters[i];
        ExprPtr originalArgument = call.arguments[i];

        if (!parameter.layout || !parameter.layout->HasOpaque() || parameter.nativeOpaque)
        {
            TypeDenoterPtr argumentType = originalArgument->GetTypeDenoter();
            LoweredExpression lowered = LowerExpression(originalArgument, nullptr, nullptr);
            if (!lowered.prelude.empty())
                FlushPendingArguments(newArguments, pendingArguments, result.prelude);
            Append(result.prelude, lowered.prelude);
            if (lowered.residual)
            {
                newArguments.push_back(lowered.residual);
                if (!parameter.output)
                    AddPendingArgument(pendingArguments, newArguments.size() - 1, argumentType);
            }
            else if (parameter.nativeOpaque && parameter.layout && parameter.layout->root->kind == LayoutNode::Kind::Array && i < callPlan.arguments.size())
            {
                std::vector<std::size_t> nativeArrayLanes;
                for (std::size_t lane = 0; lane < callPlan.arguments[i].bindings.size(); ++lane)
                    nativeArrayLanes.push_back(lane);
                newArguments.push_back(CollapseArrayLane(callPlan.arguments[i], nativeArrayLanes, &call));
            }
            continue;
        }

        TypeDenoterPtr argumentType = originalArgument->GetTypeDenoter();
        LoweredExpression lowered = LowerExpression(originalArgument, parameter.layout, parameter.layout->root);
        if (!lowered.prelude.empty())
            FlushPendingArguments(newArguments, pendingArguments, result.prelude);
        Append(result.prelude, lowered.prelude);
        if (paramABI.keepResidual && lowered.residual)
        {
            newArguments.push_back(lowered.residual);
            if (!parameter.output)
                AddPendingArgument(pendingArguments, newArguments.size() - 1, argumentType);
        }

        if (parameter.input && i < callPlan.arguments.size())
        {
            const auto& opaqueArgument = callPlan.arguments[i];
            for (const auto laneIndex : paramABI.scalarLanes)
            {
                if (laneIndex < opaqueArgument.bindings.size())
                    newArguments.push_back(BindingExpression(opaqueArgument.bindings[laneIndex], &call));
            }
            for (const auto& group : paramABI.arrayLaneGroups)
                newArguments.push_back(CollapseArrayLane(opaqueArgument, group, &call));
        }
    }
    for (std::size_t i = contract.parameters.size(); i < call.arguments.size(); ++i)
    {
        TypeDenoterPtr argumentType = call.arguments[i]->GetTypeDenoter();
        LoweredExpression lowered = LowerExpression(call.arguments[i], nullptr, nullptr);
        if (!lowered.prelude.empty())
            FlushPendingArguments(newArguments, pendingArguments, result.prelude);
        Append(result.prelude, lowered.prelude);
        if (lowered.residual)
        {
            newArguments.push_back(lowered.residual);
            AddPendingArgument(pendingArguments, newArguments.size() - 1, argumentType);
        }
    }
    call.arguments = std::move(newArguments);

    if (abi->lowerReturn)
    {
        ExprPtr residualValue;
        if (abi->returnResidual)
        {
            const std::string tempName = "xsc_opaque_result_" + std::to_string(nextTempId_++);
            auto tempDecl = ASTFactory::MakeVarDeclStmnt(
                ASTFactory::MakeTypeSpecifier(contract.originalReturnType->typeDenoter->Copy()), tempName
            );
            VarDecl* tempVar = tempDecl->varDecls.front().get();
            result.prelude.push_back(tempDecl);
            residualValue = ASTFactory::MakeObjectExpr(tempVar);
            call.arguments.push_back(ASTFactory::MakeObjectExpr(tempVar));
        }
        auto callStatement = std::make_shared<ExprStmnt>(SourcePosition::ignore);
        callStatement->expr = expression;
        result.prelude.push_back(callStatement);
        result.residual = residualValue;
        return result;
    }

    result.residual = expression;
    return ReplaceOpaqueValue(expression, result);
}

LoweredExpression OpaqueASTLowerer::LowerTernary(
    const ExprPtr& expression,
    TernaryExpr& ternary,
    const OpaqueTypeLayoutPtr& expectedLayout,
    const LayoutNodePtr& expectedNode)
{
    LoweredExpression result;
    LoweredExpression condition = LowerExpression(ternary.condExpr, nullptr, nullptr);
    LoweredExpression thenExpr  = LowerExpression(ternary.thenExpr, expectedLayout, expectedNode);
    LoweredExpression elseExpr  = LowerExpression(ternary.elseExpr, expectedLayout, expectedNode);
    Append(result.prelude, condition.prelude);
    ternary.condExpr = condition.residual;

    if (thenExpr.prelude.empty() && elseExpr.prelude.empty())
    {
        ternary.thenExpr = thenExpr.residual;
        ternary.elseExpr = elseExpr.residual;
        result.residual = (thenExpr.residual && elseExpr.residual ? expression : ExprPtr{});
        return ReplaceOpaqueValue(expression, result);
    }

    ExprPtr residual;
    VarDecl* tempVar = nullptr;
    if (thenExpr.residual && elseExpr.residual)
    {
        TypeDenoterPtr type = expression->GetTypeDenoter();
        auto tempDecl = ASTFactory::MakeVarDeclStmnt(
            ASTFactory::MakeTypeSpecifier(type->Copy()),
            "xsc_opaque_cond_" + std::to_string(nextTempId_++)
        );
        tempVar = tempDecl->varDecls.front().get();
        result.prelude.push_back(tempDecl);
        residual = ASTFactory::MakeObjectExpr(tempVar);
        thenExpr.prelude.push_back(ASTFactory::MakeAssignStmnt(ASTFactory::MakeObjectExpr(tempVar), thenExpr.residual));
        elseExpr.prelude.push_back(ASTFactory::MakeAssignStmnt(ASTFactory::MakeObjectExpr(tempVar), elseExpr.residual));
    }

    auto ifStmnt = std::make_shared<IfStmnt>(SourcePosition::ignore);
    ifStmnt->condition = condition.residual;
    ifStmnt->bodyStmnt = MakeBlock(std::move(thenExpr.prelude));
    ifStmnt->elseStmnt = std::make_shared<ElseStmnt>(SourcePosition::ignore);
    ifStmnt->elseStmnt->bodyStmnt = MakeBlock(std::move(elseExpr.prelude));
    result.prelude.push_back(ifStmnt);
    result.residual = residual;
    return ReplaceOpaqueValue(expression, result);
}

LoweredExpression OpaqueASTLowerer::LowerBinary(const ExprPtr& expression, BinaryExpr& binary)
{
    LoweredExpression result;
    LoweredExpression lhs = LowerExpression(binary.lhsExpr, nullptr, nullptr);
    LoweredExpression rhs = LowerExpression(binary.rhsExpr, nullptr, nullptr);
    Append(result.prelude, lhs.prelude);
    if ((binary.op == BinaryOp::LogicalAnd || binary.op == BinaryOp::LogicalOr) && !rhs.prelude.empty())
    {
        auto tempDecl = ASTFactory::MakeVarDeclStmnt(DataType::Bool, "xsc_opaque_short_" + std::to_string(nextTempId_++), lhs.residual);
        VarDecl* tempVar = tempDecl->varDecls.front().get();
        result.prelude.push_back(tempDecl);
        if (rhs.residual)
            rhs.prelude.push_back(ASTFactory::MakeAssignStmnt(ASTFactory::MakeObjectExpr(tempVar), rhs.residual));
        auto ifStmnt = std::make_shared<IfStmnt>(SourcePosition::ignore);
        if (binary.op == BinaryOp::LogicalAnd)
            ifStmnt->condition = ASTFactory::MakeObjectExpr(tempVar);
        else
        {
            auto negate = std::make_shared<UnaryExpr>(SourcePosition::ignore);
            negate->op   = UnaryOp::LogicalNot;
            negate->expr = ASTFactory::MakeObjectExpr(tempVar);
            ifStmnt->condition = negate;
        }
        ifStmnt->bodyStmnt = MakeBlock(std::move(rhs.prelude));
        result.prelude.push_back(ifStmnt);
        result.residual = ASTFactory::MakeObjectExpr(tempVar);
        return result;
    }
    Append(result.prelude, rhs.prelude);
    binary.lhsExpr = lhs.residual;
    binary.rhsExpr = rhs.residual;
    result.residual = expression;
    return result;
}

LoweredExpression OpaqueASTLowerer::ReplaceOpaqueValue(const ExprPtr& expression, LoweredExpression result)
{
    auto valueIt = plan_.expressionValues.find(expression.get());
    if (valueIt == plan_.expressionValues.end())
        return result;
    const OpaqueValue& value = valueIt->second;
    if (!value.Valid() || value.node->hasResidual)
        return result;
    if (auto object = expression->As<ObjectExpr>())
    {
        VarDecl* varDecl = object->FetchVarDecl();
        if (varDecl && plan_.nativeOpaqueParameters.find(varDecl) != plan_.nativeOpaqueParameters.end())
            return result;
    }
    if (value.bindings.size() == 1)
        result.residual = BindingExpression(value.bindings.front(), expression.get());
    else
        result.residual.reset();
    return result;
}

ExprPtr OpaqueASTLowerer::CollapseArrayLane(
    const OpaqueValue& value,
    const std::vector<std::size_t>& group,
    const AST* context)
{
    if (group.empty() || group.front() >= value.bindings.size())
        RuntimeErr(R_OpaqueTypeInvalidRuntimeIndex("missing array lane"), context);

    const OpaqueBinding& first = value.bindings[group.front()];
    if (first.kind == OpaqueBinding::Kind::Resource && first.resource && !first.indices.empty())
    {
        const std::size_t axis = first.indices.size() - 1;
        for (std::size_t element = 0; element < group.size(); ++element)
        {
            if (group[element] >= value.bindings.size())
                RuntimeErr(R_OpaqueTypeInvalidRuntimeIndex("incomplete resource array"), context);
            const OpaqueBinding& binding = value.bindings[group[element]];
            if (binding.kind != OpaqueBinding::Kind::Resource || binding.resource != first.resource || binding.indices.size() != first.indices.size())
                RuntimeErr(R_OpaqueTypeInvalidRuntimeIndex("unrelated resource bindings"), context);
            for (std::size_t i = 0; i < axis; ++i)
            {
                if (binding.indices[i].dynamic != first.indices[i].dynamic ||
                    (binding.indices[i].dynamic ? binding.indices[i].expression.get() != first.indices[i].expression.get() : binding.indices[i].constant != first.indices[i].constant))
                    RuntimeErr(R_OpaqueTypeInvalidRuntimeIndex("non-cohesive resource slice"), context);
            }
            if (binding.indices[axis].dynamic || binding.indices[axis].constant != static_cast<int>(element))
                RuntimeErr(R_OpaqueTypeInvalidRuntimeIndex("non-contiguous resource slice"), context);
        }

        ExprPtr expression = ASTFactory::MakeObjectExpr(first.resource);
        if (axis > 0)
        {
            std::vector<ExprPtr> prefixIndices;
            for (std::size_t i = 0; i < axis; ++i)
            {
                const auto& index = first.indices[i];
                prefixIndices.push_back(
                    index.dynamic ? index.expression : ASTFactory::MakeLiteralExpr(DataType::Int, std::to_string(index.constant))
                );
            }
            expression = ASTFactory::MakeArrayExpr(expression, std::move(prefixIndices));
        }
        return expression;
    }

    if (first.kind == OpaqueBinding::Kind::FormalLane || first.kind == OpaqueBinding::Kind::FormalArrayLane)
    {
        auto signatureIt = synthesizedSignatures_.find(activeFunction_);
        if (signatureIt != synthesizedSignatures_.end())
        {
            auto laneIt = signatureIt->second.laneParameters.find(first.formal);
            if (laneIt != signatureIt->second.laneParameters.end() && first.laneIndex < laneIt->second.size())
            {
                VarDecl* arrayParam = laneIt->second[first.laneIndex];
                bool sameArray = (arrayParam != nullptr && !arrayParam->arrayDims.empty());
                for (const auto laneIndex : group)
                    sameArray = sameArray && laneIndex < laneIt->second.size() && laneIt->second[laneIndex] == arrayParam;
                if (sameArray)
                    return ASTFactory::MakeObjectExpr(arrayParam);
            }
        }
    }

    RuntimeErr(R_OpaqueTypeInvalidRuntimeIndex("unrelated resource bindings"), context);
    return nullptr;
}

ExprPtr OpaqueASTLowerer::BindingExpression(const OpaqueBinding& binding, const AST* context)
{
    switch (binding.kind)
    {
        case OpaqueBinding::Kind::DescriptorHeap:
        {
            if (binding.indices.size() != 1 || !binding.indices.front().expression || !binding.descriptorType)
                RuntimeErr(R_OpaqueTypeInvalidRuntimeIndex("invalid descriptor heap access"), context);

            auto expression = std::make_shared<DescriptorHeapExpr>(SourcePosition::ignore);
            expression->heap                = binding.descriptorHeap;
            expression->index               = binding.indices.front().expression;
            expression->resolvedTypeDenoter = binding.descriptorType->Copy();

            return expression;
        }
        case OpaqueBinding::Kind::Resource:
        case OpaqueBinding::Kind::ResourceArraySlice:
        {
            ExprPtr expression = ASTFactory::MakeObjectExpr(binding.resource);
            if (!binding.indices.empty())
            {
                std::vector<ExprPtr> indices;
                for (const auto& index : binding.indices)
                {
                    if (index.dynamic)
                        indices.push_back(index.expression);
                    else
                        indices.push_back(ASTFactory::MakeLiteralExpr(DataType::Int, std::to_string(index.constant)));
                }
                expression = ASTFactory::MakeArrayExpr(expression, std::move(indices));
            }
            return expression;
        }
        case OpaqueBinding::Kind::FormalLane:
        case OpaqueBinding::Kind::FormalArrayLane:
        {
            auto signatureIt = synthesizedSignatures_.find(activeFunction_);
            if (signatureIt != synthesizedSignatures_.end())
            {
                auto laneIt = signatureIt->second.laneParameters.find(binding.formal);
                if (laneIt != signatureIt->second.laneParameters.end() && binding.laneIndex < laneIt->second.size())
                {
                    VarDecl* laneParam = laneIt->second[binding.laneIndex];
                    if (laneParam)
                    {
                        ExprPtr expression = ASTFactory::MakeObjectExpr(laneParam);
                        if (binding.kind == OpaqueBinding::Kind::FormalArrayLane && !binding.indices.empty())
                        {
                            std::vector<ExprPtr> indices;
                            for (const auto& index : binding.indices)
                                indices.push_back(index.dynamic ? index.expression : ASTFactory::MakeLiteralExpr(DataType::Int, std::to_string(index.constant)));
                            expression = ASTFactory::MakeArrayExpr(expression, std::move(indices));
                        }
                        return expression;
                    }
                }
            }
            /* A native plain opaque input parameter has no synthesized lane. */
            if (binding.formal)
            {
                ExprPtr expression = ASTFactory::MakeObjectExpr(binding.formal);
                if (!binding.indices.empty())
                {
                    std::vector<ExprPtr> indices;
                    for (const auto& index : binding.indices)
                        indices.push_back(index.dynamic ? index.expression : ASTFactory::MakeLiteralExpr(DataType::Int, std::to_string(index.constant)));
                    expression = ASTFactory::MakeArrayExpr(expression, std::move(indices));
                }
                return expression;
            }
            break;
        }
        case OpaqueBinding::Kind::Uninitialized:
            RuntimeErr(R_OpaqueStructUninitialized("opaque value"), context);
            break;
        case OpaqueBinding::Kind::Conflict:
            RuntimeErr(R_OpaqueStructAmbiguousAlias("opaque value"), context);
            break;
        case OpaqueBinding::Kind::FixedElementTable:
            RuntimeErr(R_OpaqueTypeInvalidRuntimeIndex("fixed element table"), context);
            break;
    }
    return nullptr;
}

void OpaqueASTLowerer::AddPendingArgument(
    std::vector<PendingArgument>& pending,
    std::size_t argumentIndex,
    const TypeDenoterPtr& type)
{
    if (!type || type->GetAliased().IsArray() || Converter::IsOpaqueTypeDenoter(type))
        return;
    PendingArgument argument;
    argument.argumentIndex = argumentIndex;
    argument.type          = type;
    pending.push_back(argument);
}

void OpaqueASTLowerer::FlushPendingArguments(
    std::vector<ExprPtr>& arguments,
    std::vector<PendingArgument>& pending,
    std::vector<StmntPtr>& prelude)
{
    for (const auto& argument : pending)
    {
        if (argument.argumentIndex >= arguments.size() || !arguments[argument.argumentIndex])
            continue;
        auto temp = ASTFactory::MakeVarDeclStmnt(
            ASTFactory::MakeTypeSpecifier(argument.type->Copy()),
            "xsc_opaque_arg_" + std::to_string(nextTempId_++),
            arguments[argument.argumentIndex]
        );
        VarDecl* tempVar = temp->varDecls.front().get();
        prelude.push_back(temp);
        arguments[argument.argumentIndex] = ASTFactory::MakeObjectExpr(tempVar);
    }
    pending.clear();
}


/* ----- Declaration stripping ----- */

void OpaqueASTLowerer::StripTypeDeclarations()
{
    std::unordered_set<StructDecl*> fullyOpaqueStructs;
    std::unordered_set<AliasDecl*> fullyOpaqueAliases;

    /* Snapshot alias layouts before any member list is mutated. */
    for (const auto& statement : program_.globalStmnts)
    {
        if (auto aliases = statement->As<AliasDeclStmnt>())
        {
            for (const auto& alias : aliases->aliasDecls)
            {
                auto layout = layouts_.Get(alias->typeDenoter, alias.get());
                if (layout && layout->HasOpaque() && !layout->HasResidual())
                    fullyOpaqueAliases.insert(alias.get());
            }
        }
    }

    for (auto& statement : program_.globalStmnts)
    {
        auto basic = statement->As<BasicDeclStmnt>();
        auto structDecl = (basic && basic->declObject ? basic->declObject->As<StructDecl>() : nullptr);
        if (!structDecl)
            continue;
        auto structType = std::make_shared<StructTypeDenoter>(structDecl);
        auto layout = layouts_.Get(structType, structDecl);
        StripStructMembers(*structDecl);
        if (layout && layout->HasOpaque() && !layout->HasResidual())
            fullyOpaqueStructs.insert(structDecl);
    }

    program_.globalStmnts.erase(
        std::remove_if(
            program_.globalStmnts.begin(), program_.globalStmnts.end(),
            [&](const StmntPtr& statement)
            {
                if (auto basic = statement->As<BasicDeclStmnt>())
                {
                    if (auto structDecl = (basic->declObject ? basic->declObject->As<StructDecl>() : nullptr))
                        return fullyOpaqueStructs.find(structDecl) != fullyOpaqueStructs.end();
                }
                if (auto aliases = statement->As<AliasDeclStmnt>())
                {
                    aliases->aliasDecls.erase(
                        std::remove_if(
                            aliases->aliasDecls.begin(), aliases->aliasDecls.end(),
                            [&](const AliasDeclPtr& alias)
                            {
                                return fullyOpaqueAliases.find(alias.get()) != fullyOpaqueAliases.end();
                            }
                        ),
                        aliases->aliasDecls.end()
                    );
                    return aliases->aliasDecls.empty() && !aliases->structDecl;
                }
                return false;
            }
        ),
        program_.globalStmnts.end()
    );
}

void OpaqueASTLowerer::StripStructMembers(StructDecl& structDecl)
{
    for (auto it = structDecl.varMembers.begin(); it != structDecl.varMembers.end(); )
    {
        auto& memberStatement = *it;
        memberStatement->varDecls.erase(
            std::remove_if(
                memberStatement->varDecls.begin(), memberStatement->varDecls.end(),
                [&](const VarDeclPtr& member)
                {
                    auto layout = layouts_.Get(member.get(), member.get());
                    return layout && layout->HasOpaque() && !layout->HasResidual();
                }
            ),
            memberStatement->varDecls.end()
        );
        if (memberStatement->varDecls.empty())
            it = structDecl.varMembers.erase(it);
        else
            ++it;
    }

    structDecl.localStmnts.erase(
        std::remove_if(
            structDecl.localStmnts.begin(), structDecl.localStmnts.end(),
            [&](const StmntPtr& statement)
            {
                auto member = statement->As<VarDeclStmnt>();
                if (!member)
                    return false;
                return std::find_if(
                    structDecl.varMembers.begin(), structDecl.varMembers.end(),
                    [&](const VarDeclStmntPtr& entry) { return entry.get() == member; }
                ) == structDecl.varMembers.end();
            }
        ),
        structDecl.localStmnts.end()
    );
}


/* ----- Small AST construction helpers ----- */

StmntPtr OpaqueASTLowerer::MakeBlock(std::vector<StmntPtr> statements)
{
    auto block = std::make_shared<CodeBlockStmnt>(SourcePosition::ignore);
    block->codeBlock = std::make_shared<CodeBlock>(SourcePosition::ignore);
    block->codeBlock->stmnts = std::move(statements);
    return block;
}

std::vector<StmntPtr> OpaqueASTLowerer::SingleStatement(const StmntPtr& statement)
{
    return std::vector<StmntPtr>(1, statement);
}

void OpaqueASTLowerer::Append(std::vector<StmntPtr>& destination, const std::vector<StmntPtr>& source)
{
    destination.insert(destination.end(), source.begin(), source.end());
}


} // /namespace OpaqueLowering

} // /namespace Xsc



// ================================================================================
