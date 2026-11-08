/*
 * ReferenceAnalyzer.cpp
 * 
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include "ReferenceAnalyzer.h"
#include "Exception.h"
#include "ReportHandler.h"
#include "AST.h"
#include "ReportIdents.h"
#include <algorithm>


namespace Xsc
{


/*
 * Internal functions
 */

// Returns true if the specified intrinsic is unavailable in every ray tracing stage (implicit derivatives, clip and group barriers).
static bool IsUnavailableInRayTracingStages(const Intrinsic t)
{
    using T = Intrinsic;

    switch (t)
    {
        case T::DDX:
        case T::DDXCoarse:
        case T::DDXFine:
        case T::DDY:
        case T::DDYCoarse:
        case T::DDYFine:
        case T::FWidth:
        case T::Clip:
        case T::AllMemoryBarrier:
        case T::AllMemoryBarrierWithGroupSync:
        case T::GroupMemoryBarrier:
        case T::GroupMemoryBarrierWithGroupSync:
        case T::DeviceMemoryBarrierWithGroupSync:
        case T::Texture_QueryLod:
        case T::Texture_QueryLodUnclamped:
        case T::Tex1D_2:
        case T::Tex1DBias:
        case T::Tex1DProj:
        case T::Tex2D_2:
        case T::Tex2DBias:
        case T::Tex2DProj:
        case T::Tex3D_2:
        case T::Tex3DBias:
        case T::Tex3DProj:
        case T::TexCube_2:
        case T::TexCubeBias:
        case T::TexCubeProj:
            return true;
        default:
            return
            (
                (t >= T::Texture_Sample_2     && t <= T::Texture_Sample_5    ) ||
                (t >= T::Texture_SampleBias_3 && t <= T::Texture_SampleBias_6) ||
                (t >= T::Texture_SampleCmp_3  && t <= T::Texture_SampleCmp_6 )
            );
    }
}

// Returns true if the specified intrinsic can be called in the specified stage, as far as the ray tracing rules go.
static bool IsIntrinsicAvailableInStage(const Intrinsic t, const ShaderTarget stage)
{
    using T = Intrinsic;
    using S = ShaderTarget;

    auto isOneOf = [stage](std::initializer_list<ShaderTarget> stages)
    {
        return (std::find(stages.begin(), stages.end(), stage) != stages.end());
    };

    switch (t)
    {
        case T::DispatchRaysIndex:
        case T::DispatchRaysDimensions:
            return IsRayTracingShaderTarget(stage);

        case T::TraceRay:
            return isOneOf({ S::RayGenerationShader, S::ClosestHitShader, S::MissShader });

        case T::CallShader:
            return isOneOf({ S::RayGenerationShader, S::ClosestHitShader, S::MissShader, S::CallableShader });

        case T::WorldRayOrigin:
        case T::WorldRayDirection:
        case T::RayTMin:
        case T::RayTCurrent:
        case T::RayFlags:
            return isOneOf({ S::IntersectionShader, S::AnyHitShader, S::ClosestHitShader, S::MissShader });

        case T::InstanceIndex:
        case T::InstanceID:
        case T::GeometryIndex:
        case T::PrimitiveIndex:
        case T::ObjectRayOrigin:
        case T::ObjectRayDirection:
        case T::ObjectToWorld3x4:
        case T::ObjectToWorld4x3:
        case T::WorldToObject3x4:
        case T::WorldToObject4x3:
        case T::ObjectToWorld:
        case T::WorldToObject:
            return isOneOf({ S::IntersectionShader, S::AnyHitShader, S::ClosestHitShader });

        case T::HitKind:
            return isOneOf({ S::AnyHitShader, S::ClosestHitShader });

        case T::ReportHit:
            return isOneOf({ S::IntersectionShader });

        case T::IgnoreHit:
        case T::AcceptHitAndEndSearch:
            return isOneOf({ S::AnyHitShader });

        case T::RayTraceClosest:
        case T::RayTraceAny:
            return true;

        default:
            return !(IsRayTracingShaderTarget(stage) && IsUnavailableInRayTracingStages(t));
    }
}

// Returns true if the specified intrinsic can't be called from an inline ray tracing callback, because traversal can't nest.
static bool IsUnavailableInRayTracingCallbacks(const Intrinsic t)
{
    return (t == Intrinsic::RayTraceClosest || t == Intrinsic::RayTraceAny || t == Intrinsic::TraceRay || t == Intrinsic::CallShader || t == Intrinsic::Clip);
}


/*
 * ReferenceAnalyzer class
 */

ReferenceAnalyzer::ReferenceAnalyzer(Log* log) :
    reportHandler_ { log }
{
}

bool ReferenceAnalyzer::ValidateRayTracing(Program& program, const ShaderTarget shaderTarget)
{
    validateRayTracing_ = true;

    try
    {
        MarkReferencesFromEntryPoint(program, shaderTarget);
    }
    catch (const ASTRuntimeError& e)
    {
        ErrorRayTracing(e.what(), e.GetAST());
    }
    catch (const std::exception& e)
    {
        ErrorRayTracing(e.what(), nullptr);
    }

    validateRayTracing_ = false;

    return !reportHandler_.HasErros();
}

void ReferenceAnalyzer::MarkReferencesFromEntryPoint(Program& program, const ShaderTarget shaderTarget)
{
    program_        = (&program);
    shaderTarget_   = shaderTarget;

    /* Visit all entry points */
    Visit(program.entryPointRef);
    Visit(program.layoutTessControl.patchConstFunctionRef);

    /* Visit all uniforms. We always wish to maintain the same uniform interface between shaders, so don't optimize these out even if unused. */
    for(const auto& stmt : program.globalStmnts)
    {
        if (const auto& basicDeclStmnt = stmt->As<BasicDeclStmnt>())
        {
            if(basicDeclStmnt->declObject != nullptr && basicDeclStmnt->declObject->As<UniformBufferDecl>())
                Visit(basicDeclStmnt);
        }
        else if (const auto& bufferDeclStmnt = stmt->As<BufferDeclStmnt>())
            Visit(bufferDeclStmnt);
        else if (const auto& samplerDeclStmnt = stmt->As<SamplerDeclStmnt>())
            Visit(samplerDeclStmnt);
    }

    /* Visit types required by compiler-generated bindless declarations. */
    for (const auto& resource : program.bindlessResourceTypes)
        MarkTypeDenoterReferences(resource.type.get());
}


/*
 * ======= Private: =======
 */

bool ReferenceAnalyzer::Reachable(AST* ast)
{
    if (!ast)
        return false;

    const bool firstVisit = ast->flags.SetOnce(AST::isReachable);

    /* Inside a ray tracing callback, visit each node once more: the callback rules don't apply to the earlier visit */
    if (!callbackStack_.empty())
        return (ast->flags.SetOnce(AST::isReachableFromRayTracingCallback) || firstVisit);

    return firstVisit;
}

void ReferenceAnalyzer::ErrorRayTracing(const std::string& msg, const AST* ast)
{
    HintCallStack();
    reportHandler_.SubmitReport(
        false, ReportTypes::Error, R_ContextError, msg,
        (program_ ? program_->sourceCode.get() : nullptr),
        (ast ? ast->area : SourceArea::ignore)
    );
}

void ReferenceAnalyzer::HintCallStack()
{
    if (callExprStack_.empty())
        return;

    ReportHandler::HintForNextReport(R_CallStack + ":");
    for (auto funcCall : callExprStack_)
    {
        /* Inline ray tracing calls have no function declaration */
        const auto funcDecl = funcCall->GetFunctionDecl();
        const auto name = (funcDecl ? funcDecl->ToString(false) : funcCall->ident);
        ReportHandler::HintForNextReport("  '" + name + "' (" + funcCall->area.Pos().ToString() + ")");
    }
}

void ReferenceAnalyzer::CheckRayTracingIntrinsic(const Intrinsic intrinsic, const std::string& ident, const AST* ast)
{
    if (!IsIntrinsicAvailableInStage(intrinsic, shaderTarget_))
        ErrorRayTracing(R_IntrinsicNotAllowedInStage(ident, ToString(shaderTarget_)), ast);
    else if (!callbackStack_.empty() && IsUnavailableInRayTracingCallbacks(intrinsic))
        ErrorRayTracing(R_IntrinsicNotAllowedInCallback(ident, callbackStack_.back()->structDeclRef->ident + "::" + callbackStack_.back()->ident), ast);
}

void ReferenceAnalyzer::VisitInlineRayTraceCallbacks(CallExpr* callExpr)
{
    if (callExpr->arguments.size() != 5)
        return;

    /* Find the callback methods the analyzer recorded for the callbacks structure */
    const auto structTypeDen = callExpr->arguments[4]->GetTypeDenoter()->GetAliased().As<StructTypeDenoter>();
    const auto structDecl = (structTypeDen ? structTypeDen->structDeclRef : nullptr);

    for (const auto& usage : program_->inlineRayTraces)
    {
        if (usage.intrinsic != callExpr->intrinsic || usage.callbacksRef == nullptr || usage.callbacksRef != structDecl)
            continue;

        Visit(structDecl);

        callExprStack_.push_back(callExpr);
        {
            for (auto method : { usage.anyHitRef, usage.intersectionRef })
            {
                if (method)
                {
                    callbackStack_.push_back(method);
                    Visit(method);
                    callbackStack_.pop_back();
                }
            }
        }
        callExprStack_.pop_back();
        break;
    }
}

void ReferenceAnalyzer::MarkTypeDenoterReferences(const TypeDenoter* typeDenoter)
{
    if (!typeDenoter)
        return;

    Visit(typeDenoter->SymbolRef());

    const auto& aliasedTypeDenoter = typeDenoter->GetAliased();
    if (&aliasedTypeDenoter != typeDenoter)
        Visit(aliasedTypeDenoter.SymbolRef());

    MarkTypeDenoterReferences(aliasedTypeDenoter.FetchSubTypeDenoter());
}

void ReferenceAnalyzer::VisitStmntList(const std::vector<StmntPtr>& stmnts)
{
    for (auto& stmnt : stmnts)
    {
        if (!stmnt->flags(AST::isDeadCode))
            Visit(stmnt);
    }
}

void ReferenceAnalyzer::MarkLValueExpr(const Expr* expr)
{
    if (expr)
    {
        if (auto objectExpr = expr->As<ObjectExpr>())
            MarkLValueExprObject(objectExpr);
        else if (auto bracketExpr = expr->As<BracketExpr>())
            MarkLValueExpr(bracketExpr->expr.get());
        else if (auto arrayExpr = expr->As<ArrayExpr>())
            MarkLValueExpr(arrayExpr->prefixExpr.get());
    }
}

void ReferenceAnalyzer::MarkLValueExprObject(const ObjectExpr* objectExpr)
{
    if (objectExpr)
    {
        /* Mark prefix expression as l-value */
        MarkLValueExpr(objectExpr->prefixExpr.get());

        if (auto symbol = objectExpr->symbolRef)
        {
            /* Mark symbol that it is written to */
            symbol->flags << Decl::isWrittenTo;
        }
    }
}

/* ------- Visit functions ------- */

#define IMPLEMENT_VISIT_PROC(AST_NAME) \
    void ReferenceAnalyzer::Visit##AST_NAME(AST_NAME* ast, void* args)

IMPLEMENT_VISIT_PROC(CodeBlock)
{
    VisitStmntList(ast->stmnts);
}

IMPLEMENT_VISIT_PROC(SwitchCase)
{
    Visit(ast->expr);
    VisitStmntList(ast->stmnts);
}

IMPLEMENT_VISIT_PROC(TypeSpecifier)
{
    if (Reachable(ast))
    {
        MarkTypeDenoterReferences(ast->typeDenoter.get());
        VISIT_DEFAULT(TypeSpecifier);
    }
}

/* --- Declarations --- */

IMPLEMENT_VISIT_PROC(VarDecl)
{
    if (Reachable(ast))
    {
        Visit(ast->declStmntRef);
        Visit(ast->bufferDeclRef);
        Visit(ast->staticMemberVarRef);
        VISIT_DEFAULT(VarDecl);
    }
}

IMPLEMENT_VISIT_PROC(StructDecl)
{
    if (Reachable(ast))
    {
        /* Only visit member variables (functions must only be visited by a call expression) */
        Visit(ast->varMembers);
        Reachable(ast->declStmntRef);
    }
}

IMPLEMENT_VISIT_PROC(BufferDecl)
{
    if (Reachable(ast))
        Visit(ast->declStmntRef);
}

IMPLEMENT_VISIT_PROC(SamplerDecl)
{
    if (Reachable(ast))
        Visit(ast->declStmntRef);
}

IMPLEMENT_VISIT_PROC(FunctionDecl)
{
    if (Reachable(ast))
    {
        /* Is the forward declaration connected to its function implementation? */
        if (ast->IsForwardDecl())
        {
            if (ast->funcImplRef)
                Visit(ast->funcImplRef);
            else
                RuntimeErr(R_MissingFuncImpl(ast->ToString(false)), ast);
        }
        else
        {
            /* Visit all forward declarations */
            for (auto funcForwardDecl : ast->funcForwardDeclRefs)
                Visit(funcForwardDecl);
        }

        PushFunctionDecl(ast);
        {
            VISIT_DEFAULT(FunctionDecl);
        }
        PopFunctionDecl();

        /* Mark parent node as reachable */
        Reachable(ast->declStmntRef);
    }
}

IMPLEMENT_VISIT_PROC(UniformBufferDecl)
{
    if (Reachable(ast))
    {
        VISIT_DEFAULT(UniformBufferDecl);
        Reachable(ast->declStmntRef);
    }
}

IMPLEMENT_VISIT_PROC(VarDeclStmnt)
{
    if (Reachable(ast))
        VISIT_DEFAULT(VarDeclStmnt);
}

/* --- Declaration statements --- */

IMPLEMENT_VISIT_PROC(BufferDeclStmnt)
{
    if (Reachable(ast))
    {
        MarkTypeDenoterReferences(ast->typeDenoter->genericTypeDenoter.get());
        VISIT_DEFAULT(BufferDeclStmnt);
    }
}

IMPLEMENT_VISIT_PROC(SamplerDeclStmnt)
{
    Reachable(ast);
}

/* --- Statements --- */

IMPLEMENT_VISIT_PROC(CtrlTransferStmnt)
{
    if (validateRayTracing_ && ast->transfer == CtrlTransfer::Discard)
    {
        if (IsRayTracingShaderTarget(shaderTarget_))
            ErrorRayTracing(R_IntrinsicNotAllowedInStage("discard", ToString(shaderTarget_)), ast);
        else if (!callbackStack_.empty())
            ErrorRayTracing(R_IntrinsicNotAllowedInCallback("discard", callbackStack_.back()->structDeclRef->ident + "::" + callbackStack_.back()->ident), ast);
    }

    VISIT_DEFAULT(CtrlTransferStmnt);
}

/* --- Expressions --- */

IMPLEMENT_VISIT_PROC(UnaryExpr)
{
    if (IsLValueOp(ast->op))
        MarkLValueExpr(ast->expr.get());
    VISIT_DEFAULT(UnaryExpr);
}

IMPLEMENT_VISIT_PROC(PostUnaryExpr)
{
    if (IsLValueOp(ast->op))
        MarkLValueExpr(ast->expr.get());
    VISIT_DEFAULT(PostUnaryExpr);
}

IMPLEMENT_VISIT_PROC(CallExpr)
{
    /* Check the ray tracing rules, then visit the callback methods of inline ray tracing calls as if they were called here */
    if (validateRayTracing_ && ast->intrinsic != Intrinsic::Undefined)
    {
        CheckRayTracingIntrinsic(ast->intrinsic, ast->ident, ast);

        if (IsInlineRayTracingIntrinsic(ast->intrinsic) && callbackStack_.empty())
            VisitInlineRayTraceCallbacks(ast);
    }

    /* Don't use forward declaration for call stack */
    if (auto funcDecl = ast->GetFunctionImpl())
    {
        /* Check for recursive calls (if function is already on the call stack) */
        auto funcCallIt = std::find_if(
            callExprStack_.begin(), callExprStack_.end(),
            [funcDecl](CallExpr* callExpr)
            {
                return (callExpr->GetFunctionImpl() == funcDecl);
            }
        );

        if (funcCallIt != callExprStack_.end())
        {
            /* Pass call stack to report handler */
            HintCallStack();

            /* Throw error message of recursive call */
            RuntimeErr(R_IllegalRecursiveCall(funcDecl->ToString()), ast);
        }

        /* Mark function declaration as referenced */
        callExprStack_.push_back(ast);
        {
            Visit(funcDecl);
        }
        callExprStack_.pop_back();

        /* Mark owner struct as referenced */
        if (funcDecl)
        {
            if (auto structDecl = funcDecl->structDeclRef)
                Visit(structDecl);
        }
    }

    if (ast->intrinsic != Intrinsic::Undefined)
    {
        /* Mark RW buffers used in read operations */
        if ( ( ast->intrinsic >= Intrinsic::Image_AtomicAdd && ast->intrinsic <= Intrinsic::Image_AtomicExchange ) || ast->intrinsic == Intrinsic::Image_Load )
        {
            if (!ast->arguments.empty())
            {
                const auto& typeDen = ast->arguments[0]->GetTypeDenoter()->GetAliased();
                if (auto bufferTypeDen = typeDen.As<BufferTypeDenoter>())
                {
                    if (IsRWImageBufferType(bufferTypeDen->bufferType))
                    {
                        if (auto bufferDecl = bufferTypeDen->bufferDeclRef)
                            bufferDecl->flags << BufferDecl::isUsedForImageRead;
                    }
                }
            }
        }

        /* Collect all used intrinsics (if they can not be inlined) */
        if (!ast->flags(CallExpr::canInlineIntrinsicWrapper))
            program_->RegisterIntrinsicUsage(ast->intrinsic, ast->arguments);
    }

    /* Mark all arguments, that are assigned to output parameters, as l-values */
    ast->ForEachOutputArgument(
        [this](ExprPtr& argExpr)
        {
            MarkLValueExpr(argExpr.get());
        }
    );

    VISIT_DEFAULT(CallExpr);
}

IMPLEMENT_VISIT_PROC(ObjectExpr)
{
    /* Check if this symbol is the fragment coordinate (SV_Position/ gl_FragCoord) */
    if (auto varDecl = ast->FetchVarDecl())
    {
        if (varDecl->semantic == Semantic::FragCoord && shaderTarget_ == ShaderTarget::FragmentShader)
        {
            /* Mark frag-coord usage in fragment program layout */
            program_->layoutFragment.fragCoordUsed = true;
        }
    }

    /* Fetch used matrix subscripts */
    if (ast->prefixExpr)
    {
        const auto& prefixTypeDen = ast->prefixExpr->GetTypeDenoter()->GetAliased();
        if (prefixTypeDen.IsMatrix())
        {
            auto prefixBaseTypeDen = prefixTypeDen.As<BaseTypeDenoter>();
            program_->usedMatrixSubscripts.insert({ prefixBaseTypeDen->dataType, ast->ident });
        }
    }

    /* Visit symbol reference and sub nodes */
    Visit(ast->symbolRef);

    VISIT_DEFAULT(ObjectExpr);
}

IMPLEMENT_VISIT_PROC(AssignExpr)
{
    /* Mark l-value expression */
    MarkLValueExpr(ast->lvalueExpr.get());

    VISIT_DEFAULT(AssignExpr);
}

#undef IMPLEMENT_VISIT_PROC


} // /namespace Xsc



// ================================================================================
