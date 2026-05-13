/*
 * HLSLGenerator.cpp
 *
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include "HLSLGenerator.h"
#include "TypeDenoter.h"
#include "Exception.h"
#include "Helper.h"
#include "ReportIdents.h"
#include <algorithm>


namespace Xsc
{


/* Helper args structures (mirroring GLSLGenerator's pattern). */
namespace
{
    struct IfStmntArgs
    {
        bool inHasElseParentNode;
    };

    struct StructDeclArgs
    {
        bool inEndWithSemicolon;
    };
} // namespace


/* ----- Token-spelling virtuals ----- */

// Lookup table for the HLSL spelling of a Semantic enum value.
// For semantics that overlap (VertexPosition / FragCoord both map to "SV_Position"
// in HLSL4+; PointSize is HLSL3-only "PSIZE") we pick the most common modern HLSL form.
const std::string& HLSLGenerator::SemanticKeyword(Semantic s) const
{
        static const std::string undef               = "";
        static const std::string sv_ClipDistance     = "SV_ClipDistance";
        static const std::string sv_CullDistance     = "SV_CullDistance";
        static const std::string sv_Coverage         = "SV_Coverage";
        static const std::string sv_Depth            = "SV_Depth";
        static const std::string sv_DepthGE          = "SV_DepthGreaterEqual";
        static const std::string sv_DepthLE          = "SV_DepthLessEqual";
        static const std::string sv_DispatchThreadID = "SV_DispatchThreadID";
        static const std::string sv_DomainLocation   = "SV_DomainLocation";
        static const std::string sv_Position         = "SV_Position";
        static const std::string sv_GroupID          = "SV_GroupID";
        static const std::string sv_GroupIndex       = "SV_GroupIndex";
        static const std::string sv_GroupThreadID    = "SV_GroupThreadID";
        static const std::string sv_GSInstanceID     = "SV_GSInstanceID";
        static const std::string sv_InnerCoverage    = "SV_InnerCoverage";
        static const std::string sv_InsideTessFactor = "SV_InsideTessFactor";
        static const std::string sv_InstanceID       = "SV_InstanceID";
        static const std::string sv_IsFrontFace      = "SV_IsFrontFace";
        static const std::string sv_OutputCPID       = "SV_OutputControlPointID";
        static const std::string sv_PrimitiveID      = "SV_PrimitiveID";
        static const std::string sv_RTAI             = "SV_RenderTargetArrayIndex";
        static const std::string sv_SampleIndex      = "SV_SampleIndex";
        static const std::string sv_StencilRef       = "SV_StencilRef";
        static const std::string sv_Target           = "SV_Target";
        static const std::string sv_TessFactor       = "SV_TessFactor";
        static const std::string sv_VertexID         = "SV_VertexID";
        static const std::string sv_ViewportArrayIdx = "SV_ViewportArrayIndex";
        static const std::string psize               = "PSIZE";

        switch (s)
        {
            case Semantic::ClipDistance:           return sv_ClipDistance;
            case Semantic::CullDistance:           return sv_CullDistance;
            case Semantic::Coverage:               return sv_Coverage;
            case Semantic::Depth:                  return sv_Depth;
            case Semantic::DepthGreaterEqual:      return sv_DepthGE;
            case Semantic::DepthLessEqual:         return sv_DepthLE;
            case Semantic::DispatchThreadID:       return sv_DispatchThreadID;
            case Semantic::DomainLocation:         return sv_DomainLocation;
            case Semantic::FragCoord:              return sv_Position;
            case Semantic::GroupID:                return sv_GroupID;
            case Semantic::GroupIndex:             return sv_GroupIndex;
            case Semantic::GroupThreadID:          return sv_GroupThreadID;
            case Semantic::GSInstanceID:           return sv_GSInstanceID;
            case Semantic::InnerCoverage:          return sv_InnerCoverage;
            case Semantic::InsideTessFactor:       return sv_InsideTessFactor;
            case Semantic::InstanceID:             return sv_InstanceID;
            case Semantic::IsFrontFace:            return sv_IsFrontFace;
            case Semantic::OutputControlPointID:   return sv_OutputCPID;
            case Semantic::PointSize:              return psize;
            case Semantic::PrimitiveID:            return sv_PrimitiveID;
            case Semantic::RenderTargetArrayIndex: return sv_RTAI;
            case Semantic::SampleIndex:            return sv_SampleIndex;
            case Semantic::StencilRef:             return sv_StencilRef;
            case Semantic::Target:                 return sv_Target;
            case Semantic::TessFactor:             return sv_TessFactor;
            case Semantic::VertexID:               return sv_VertexID;
            case Semantic::VertexPosition:         return sv_Position;
            case Semantic::ViewportArrayIndex:     return sv_ViewportArrayIdx;
            default:                               return undef;
        }
    }

// HLSL spelling for an AttributeType enum value.
const std::string& HLSLGenerator::AttributeTypeKeyword(AttributeType t) const
{
        static const std::string undef               = "";
        static const std::string a_branch            = "branch";
        static const std::string a_call              = "call";
        static const std::string a_flatten           = "flatten";
        static const std::string a_ifAll             = "ifAll";
        static const std::string a_ifAny             = "ifAny";
        static const std::string a_isolate           = "isolate";
        static const std::string a_loop              = "loop";
        static const std::string a_maxexports        = "maxexports";
        static const std::string a_maxInstr          = "maxInstructionCount";
        static const std::string a_maxTempReg        = "maxtempreg";
        static const std::string a_noExprOpt         = "noExpressionOptimizations";
        static const std::string a_predicate         = "predicate";
        static const std::string a_predBlock         = "predicateBlock";
        static const std::string a_reduceTempReg     = "reduceTempRegUsage";
        static const std::string a_removeUnusedInp   = "removeUnusedInputs";
        static const std::string a_sampreg           = "sampreg";
        static const std::string a_unroll            = "unroll";
        static const std::string a_unused            = "unused";
        static const std::string a_xps               = "xps";

        static const std::string a_domain            = "domain";
        static const std::string a_earlyDepth        = "earlydepthstencil";
        static const std::string a_instance          = "instance";
        static const std::string a_maxTessFactor     = "maxtessfactor";
        static const std::string a_maxVertexCount    = "maxvertexcount";
        static const std::string a_numthreads        = "numthreads";
        static const std::string a_outputCP          = "outputcontrolpoints";
        static const std::string a_outputTop         = "outputtopology";
        static const std::string a_partitioning      = "partitioning";
        static const std::string a_patchSize         = "patchsize";
        static const std::string a_patchConstFunc    = "patchconstantfunc";

        switch (t)
        {
            case AttributeType::Branch:                    return a_branch;
            case AttributeType::Call:                      return a_call;
            case AttributeType::Flatten:                   return a_flatten;
            case AttributeType::IfAll:                     return a_ifAll;
            case AttributeType::IfAny:                     return a_ifAny;
            case AttributeType::Isolate:                   return a_isolate;
            case AttributeType::Loop:                      return a_loop;
            case AttributeType::MaxExports:                return a_maxexports;
            case AttributeType::MaxInstructionCount:       return a_maxInstr;
            case AttributeType::MaxTempReg:                return a_maxTempReg;
            case AttributeType::NoExpressionOptimizations: return a_noExprOpt;
            case AttributeType::Predicate:                 return a_predicate;
            case AttributeType::PredicateBlock:            return a_predBlock;
            case AttributeType::ReduceTempRegUsage:        return a_reduceTempReg;
            case AttributeType::RemoveUnusedInputs:        return a_removeUnusedInp;
            case AttributeType::SampReg:                   return a_sampreg;
            case AttributeType::Unroll:                    return a_unroll;
            case AttributeType::Unused:                    return a_unused;
            case AttributeType::Xps:                       return a_xps;
            case AttributeType::Domain:                    return a_domain;
            case AttributeType::EarlyDepthStencil:         return a_earlyDepth;
            case AttributeType::Instance:                  return a_instance;
            case AttributeType::MaxTessFactor:             return a_maxTessFactor;
            case AttributeType::MaxVertexCount:            return a_maxVertexCount;
            case AttributeType::NumThreads:                return a_numthreads;
            case AttributeType::OutputControlPoints:       return a_outputCP;
            case AttributeType::OutputTopology:            return a_outputTop;
            case AttributeType::Partitioning:              return a_partitioning;
            case AttributeType::PatchSize:                 return a_patchSize;
            case AttributeType::PatchConstantFunc:         return a_patchConstFunc;
            default:                                       return undef;
        }
    }

// HLSL spelling for a StorageClass.
const std::string& HLSLGenerator::StorageClassKeyword(StorageClass s) const
{
        static const std::string undef        = "";
        static const std::string s_extern     = "extern";
        static const std::string s_precise    = "precise";
        static const std::string s_shared     = "shared";
        static const std::string s_grpshared  = "groupshared";
        static const std::string s_static     = "static";
        static const std::string s_volatile   = "volatile";

        switch (s)
        {
            case StorageClass::Extern:      return s_extern;
            case StorageClass::Precise:     return s_precise;
            case StorageClass::Shared:      return s_shared;
            case StorageClass::GroupShared: return s_grpshared;
            case StorageClass::Static:      return s_static;
            case StorageClass::Volatile:    return s_volatile;
            default:                        return undef;
        }
    }

// HLSL spelling for an InterpModifier.
const std::string& HLSLGenerator::InterpModifierKeyword(InterpModifier m) const
{
        static const std::string undef       = "";
        static const std::string m_nointerp  = "nointerpolation";
        static const std::string m_linear    = "linear";
        static const std::string m_centroid  = "centroid";
        static const std::string m_noperspe  = "noperspective";
        static const std::string m_sample    = "sample";

        switch (m)
        {
            case InterpModifier::NoInterpolation: return m_nointerp;
            case InterpModifier::Linear:          return m_linear;
            case InterpModifier::Centroid:        return m_centroid;
            case InterpModifier::NoPerspective:   return m_noperspe;
            case InterpModifier::Sample:          return m_sample;
            default:                              return undef;
        }
    }

// HLSL spelling for a TypeModifier.
const std::string& HLSLGenerator::TypeModifierKeyword(TypeModifier m) const
{
        static const std::string undef       = "";
        static const std::string m_const     = "const";
        static const std::string m_rowmajor  = "row_major";
        static const std::string m_colmajor  = "column_major";
        static const std::string m_snorm     = "snorm";
        static const std::string m_unorm     = "unorm";

        switch (m)
        {
            case TypeModifier::Const:       return m_const;
            case TypeModifier::RowMajor:    return m_rowmajor;
            case TypeModifier::ColumnMajor: return m_colmajor;
            case TypeModifier::SNorm:       return m_snorm;
            case TypeModifier::UNorm:       return m_unorm;
            default:                        return undef;
        }
    }

// HLSL spelling for a SamplerType.
const std::string& HLSLGenerator::SamplerTypeKeyword(SamplerType t) const
{
        static const std::string undef                     = "";
        static const std::string s_sampler1D               = "sampler1D";
        static const std::string s_sampler2D               = "sampler2D";
        static const std::string s_sampler3D               = "sampler3D";
        static const std::string s_samplerCUBE             = "samplerCUBE";
        static const std::string s_sampler1DShadow         = "sampler1DShadow";
        static const std::string s_sampler2DShadow         = "sampler2DShadow";
        static const std::string s_samplerState            = "SamplerState";
        static const std::string s_samplerCmpState         = "SamplerComparisonState";

        switch (t)
        {
            case SamplerType::Sampler1D:              return s_sampler1D;
            case SamplerType::Sampler2D:              return s_sampler2D;
            case SamplerType::Sampler3D:              return s_sampler3D;
            case SamplerType::SamplerCube:            return s_samplerCUBE;
            case SamplerType::Sampler1DShadow:        return s_sampler1DShadow;
            case SamplerType::Sampler2DShadow:        return s_sampler2DShadow;
            case SamplerType::SamplerState:           return s_samplerState;
            case SamplerType::SamplerComparisonState: return s_samplerCmpState;
            default:                                  return undef;
        }
    }

// HLSL spelling for a BufferType (e.g. "StructuredBuffer", "RWTexture2D").
// Delegates to BufferTypeToString in ASTEnums.cpp so the HLSL backend stays in
// sync with the shared lookup. Returns by value (BufferTypeToString does the same).
std::string HLSLGenerator::BufferTypeKeyword(BufferType t) const
{
    return BufferTypeToString(t);
}


/* ----- HLSLGenerator class ----- */

HLSLGenerator::HLSLGenerator(Log* log) :
    Generator { log }
{
}

void HLSLGenerator::GenerateCodePrimary(
    Program& program, const ShaderInput& inputDesc, const ShaderOutput& /*outputDesc*/)
{
    try
    {
        WriteFileHeader(inputDesc);

        /* Visit program AST as-is (no AST rewriting was performed for HLSL output) */
        Visit(&program);
    }
    catch (const Report&)
    {
        throw;
    }
    catch (const ASTRuntimeError& e)
    {
        Error(e.what(), e.GetAST());
    }
    catch (const std::exception& e)
    {
        Error(e.what());
    }
}

void HLSLGenerator::WriteFileHeader(const ShaderInput& inputDesc)
{
    WriteLn("// HLSL " + ToString(GetShaderTarget()) +
            (inputDesc.entryPoint.empty() ? std::string("") : " \"" + inputDesc.entryPoint + "\""));
    WriteLn("// Generated by XShaderCompiler");
    WriteLn("// " + TimePoint());
    Blank();
}


/* ------- Visit functions ------- */

#define IMPLEMENT_VISIT_PROC(AST_NAME) \
    void HLSLGenerator::Visit##AST_NAME(AST_NAME* ast, void* args)

IMPLEMENT_VISIT_PROC(Program)
{
    WriteStmntList(ast->globalStmnts, true);
}

IMPLEMENT_VISIT_PROC(CodeBlock)
{
    WriteScopeOpen();
    {
        WriteStmntList(ast->stmnts);
    }
    WriteScopeClose();
}

IMPLEMENT_VISIT_PROC(SwitchCase)
{
    if (ast->expr)
    {
        BeginLn();
        Write("case ");
        Visit(ast->expr);
        Write(":");
        EndLn();
    }
    else
    {
        WriteLn("default:");
    }

    IncIndent();
    {
        Visit(ast->stmnts);
    }
    DecIndent();
}

IMPLEMENT_VISIT_PROC(ArrayDimension)
{
    Write(ast->ToString());
}

IMPLEMENT_VISIT_PROC(TypeSpecifier)
{
    /* If there is an inline struct decl, emit it; otherwise emit the type denoter */
    if (ast->structDecl)
        Visit(ast->structDecl);
    else if (ast->typeDenoter)
        WriteTypeDenoter(*ast->typeDenoter, ast);
}

/* --- Declarations --- */

IMPLEMENT_VISIT_PROC(VarDecl)
{
    Write(ast->ident);
    Visit(ast->arrayDims);

    /* Semantic (e.g. " : SV_Position") */
    WriteSemantic(ast->semantic);

    /* Initializer */
    if (ast->initializer)
    {
        Write(" = ");
        Visit(ast->initializer);
    }
}

IMPLEMENT_VISIT_PROC(StructDecl)
{
    PushStructDecl(ast);
    {
        bool endWithSemicolon = false;
        if (auto a = reinterpret_cast<StructDeclArgs*>(args))
            endWithSemicolon = a->inEndWithSemicolon;

        WriteStructDecl(ast, endWithSemicolon);
    }
    PopStructDecl();
}

IMPLEMENT_VISIT_PROC(SamplerDecl)
{
    /* Emit identifier + optional array dims + register annotation */
    Write(ast->ident);
    Visit(ast->arrayDims);
    WriteRegisters(ast->slotRegisters);
}

/* --- Declaration statements --- */

IMPLEMENT_VISIT_PROC(FunctionDecl)
{
    WriteFunction(ast);
    Blank();
}

IMPLEMENT_VISIT_PROC(UniformBufferDecl)
{
    BeginLn();

    /* cbuffer or tbuffer */
    if (ast->bufferType == UniformBufferType::TextureBuffer)
        Write("tbuffer ");
    else
        Write("cbuffer ");

    Write(ast->ident);

    /* Register annotation (e.g. " : register(b0)") */
    WriteRegisters(ast->slotRegisters);

    EndLn();

    /* Member variable declarations enclosed in a braced scope, followed by ';' */
    WriteScopeOpen(false, true);
    {
        PushUniformBufferDecl(ast);
        {
            Visit(ast->varMembers);
        }
        PopUniformBufferDecl();
    }
    WriteScopeClose();

    Blank();
}

IMPLEMENT_VISIT_PROC(BufferDeclStmnt)
{
    /* Buffer (texture / structured / RW) declaration statement.
       Format: <BufferType><<GenericType>> name : register(...); */
    for (auto& bufferDecl : ast->bufferDecls)
    {
        BeginLn();

        /* BufferType keyword (e.g. "Texture2D", "RWStructuredBuffer") */
        const auto bufferType = bufferDecl->GetBufferType();
        Write(BufferTypeKeyword(bufferType));

        /* Optional generic type angle-brackets (e.g. "<float4>") */
        if (ast->typeDenoter && ast->typeDenoter->genericTypeDenoter)
        {
            Write("<");
            WriteTypeDenoter(*ast->typeDenoter->genericTypeDenoter, ast);
            if (ast->typeDenoter->genericSize > 1)
            {
                Write(", ");
                Write(std::to_string(ast->typeDenoter->genericSize));
            }
            Write(">");
        }

        Write(" ");
        Write(bufferDecl->ident);
        Visit(bufferDecl->arrayDims);

        WriteRegisters(bufferDecl->slotRegisters);
        Write(";");

        EndLn();
    }

    if (InsideGlobalScope())
        Blank();
}

IMPLEMENT_VISIT_PROC(SamplerDeclStmnt)
{
    /* Format: SamplerState name : register(s0); */
    for (auto& samplerDecl : ast->samplerDecls)
    {
        BeginLn();

        if (ast->typeDenoter)
            Write(SamplerTypeKeyword(ast->typeDenoter->samplerType));
        else
            Write(SamplerTypeKeyword(samplerDecl->GetSamplerType()));

        Write(" ");
        Write(samplerDecl->ident);
        Visit(samplerDecl->arrayDims);

        WriteRegisters(samplerDecl->slotRegisters);
        Write(";");

        EndLn();
    }

    if (InsideGlobalScope())
        Blank();
}

IMPLEMENT_VISIT_PROC(VarDeclStmnt)
{
    PushVarDeclStmnt(ast);
    {
        WriteAttributes(ast->attribs);

        BeginLn();

        /* Interpolation modifiers (struct members & parameters) */
        WriteInterpModifiers(ast->typeSpecifier->interpModifiers);

        /* Storage classes (static, extern, ...) */
        WriteStorageClasses(ast->typeSpecifier->storageClasses);

        /* Input modifiers ("in"/"out"/"inout"/"uniform") for parameters */
        WriteParameterModifiers(*ast->typeSpecifier);

        /* Type modifiers (const, row_major, ...) */
        WriteTypeModifiers(ast->typeSpecifier->typeModifiers);

        /* Type itself */
        Visit(ast->typeSpecifier);
        Write(" ");

        /* Comma-separated variable declarator list */
        for (std::size_t i = 0; i < ast->varDecls.size(); ++i)
        {
            Visit(ast->varDecls[i]);
            if (i + 1 < ast->varDecls.size())
                Write(", ");
        }

        Write(";");
        EndLn();
    }
    PopVarDeclStmnt();

    if (InsideGlobalScope())
        Blank();
}

IMPLEMENT_VISIT_PROC(AliasDeclStmnt)
{
    /* Inline struct declared via typedef-like alias (rare in HLSL). For now emit
       only the struct part if present; full typedef support is out of MVP scope. */
    if (ast->structDecl && !ast->structDecl->IsAnonymous())
    {
        StructDeclArgs sa{ true };
        Visit(ast->structDecl, &sa);
    }
}

IMPLEMENT_VISIT_PROC(BasicDeclStmnt)
{
    /* A BasicDeclStmnt wraps a single Decl (StructDecl, FunctionDecl, or UniformBufferDecl) */
    if (auto structDecl = ast->declObject->As<StructDecl>())
    {
        StructDeclArgs sa{ true };
        Visit(structDecl, &sa);
    }
    else
    {
        Visit(ast->declObject);
    }
}

/* --- Statements --- */

IMPLEMENT_VISIT_PROC(NullStmnt)
{
    WriteLn(";");
}

IMPLEMENT_VISIT_PROC(CodeBlockStmnt)
{
    Visit(ast->codeBlock);
}

IMPLEMENT_VISIT_PROC(ForLoopStmnt)
{
    WriteAttributes(ast->attribs);
    BeginLn();

    Write("for (");

    PushOptions({ false, false });
    {
        if (ast->initStmnt)
            Visit(ast->initStmnt);
        else
            Write(";");
        Write(" ");
        if (ast->condition)
            Visit(ast->condition);
        Write("; ");
        if (ast->iteration)
            Visit(ast->iteration);
    }
    PopOptions();

    Write(")");

    WriteScopedStmnt(ast->bodyStmnt.get());
}

IMPLEMENT_VISIT_PROC(WhileLoopStmnt)
{
    WriteAttributes(ast->attribs);
    BeginLn();
    Write("while (");
    Visit(ast->condition);
    Write(")");
    WriteScopedStmnt(ast->bodyStmnt.get());
}

IMPLEMENT_VISIT_PROC(DoWhileLoopStmnt)
{
    WriteAttributes(ast->attribs);
    BeginLn();
    Write("do");
    WriteScopedStmnt(ast->bodyStmnt.get());

    WriteScopeContinue();
    Write("while (");
    Visit(ast->condition);
    Write(");");
    EndLn();
}

IMPLEMENT_VISIT_PROC(IfStmnt)
{
    bool hasElseParentNode = (args != nullptr ? reinterpret_cast<IfStmntArgs*>(args)->inHasElseParentNode : false);

    if (!hasElseParentNode)
    {
        WriteAttributes(ast->attribs);
        BeginLn();
    }

    Write("if (");
    Visit(ast->condition);
    Write(")");

    WriteScopedStmnt(ast->bodyStmnt.get());

    Visit(ast->elseStmnt);
}

IMPLEMENT_VISIT_PROC(ElseStmnt)
{
    if (ast->bodyStmnt && ast->bodyStmnt->Type() == AST::Types::IfStmnt)
    {
        WriteScopeContinue();
        Write("else ");

        IfStmntArgs ifArgs{ true };
        Visit(ast->bodyStmnt, &ifArgs);
    }
    else
    {
        WriteScopeContinue();
        Write("else");
        WriteScopedStmnt(ast->bodyStmnt.get());
    }
}

IMPLEMENT_VISIT_PROC(SwitchStmnt)
{
    WriteAttributes(ast->attribs);
    BeginLn();
    Write("switch (");
    Visit(ast->selector);
    Write(")");

    WriteScopeOpen();
    {
        Visit(ast->cases);
    }
    WriteScopeClose();
}

IMPLEMENT_VISIT_PROC(ExprStmnt)
{
    BeginLn();
    Visit(ast->expr);
    Write(";");
    EndLn();
}

IMPLEMENT_VISIT_PROC(ReturnStmnt)
{
    BeginLn();
    if (ast->expr)
    {
        Write("return ");
        Visit(ast->expr);
        Write(";");
    }
    else
    {
        Write("return;");
    }
    EndLn();
}

IMPLEMENT_VISIT_PROC(CtrlTransferStmnt)
{
    WriteLn(CtrlTransformToString(ast->transfer) + ";");
}

/* --- Expressions --- */

IMPLEMENT_VISIT_PROC(SequenceExpr)
{
    for (std::size_t i = 0, n = ast->exprs.size(); i < n; ++i)
    {
        Visit(ast->exprs[i]);
        if (i + 1 < n)
            Write(", ");
    }
}

IMPLEMENT_VISIT_PROC(LiteralExpr)
{
    Write(ast->value);
}

IMPLEMENT_VISIT_PROC(TypeSpecifierExpr)
{
    if (ast->typeSpecifier && ast->typeSpecifier->typeDenoter)
        WriteTypeDenoter(*ast->typeSpecifier->typeDenoter, ast);
}

IMPLEMENT_VISIT_PROC(TernaryExpr)
{
    Visit(ast->condExpr);
    Write(" ? ");
    Visit(ast->thenExpr);
    Write(" : ");
    Visit(ast->elseExpr);
}

IMPLEMENT_VISIT_PROC(BinaryExpr)
{
    Visit(ast->lhsExpr);
    Write(" " + BinaryOpToString(ast->op) + " ");
    Visit(ast->rhsExpr);
}

IMPLEMENT_VISIT_PROC(UnaryExpr)
{
    Write(UnaryOpToString(ast->op));
    Visit(ast->expr);
}

IMPLEMENT_VISIT_PROC(PostUnaryExpr)
{
    Visit(ast->expr);
    Write(UnaryOpToString(ast->op));
}

IMPLEMENT_VISIT_PROC(CallExpr)
{
    /* HLSL backend: no intrinsic rewriting. Emit the original function call as-is. */

    /* Prefix expression (for member function or texture method call: foo.bar(...)) */
    if (ast->prefixExpr)
    {
        Visit(ast->prefixExpr);
        Write(ast->isStatic ? "::" : ".");
    }

    /* For type constructors (e.g. float4(...)) the ident may be empty; emit the type instead */
    if (ast->ident.empty())
    {
        if (ast->typeDenoter)
            WriteTypeDenoter(*ast->typeDenoter, ast);
    }
    else
    {
        Write(ast->ident);
    }

    Write("(");
    for (std::size_t i = 0; i < ast->arguments.size(); ++i)
    {
        Visit(ast->arguments[i]);
        if (i + 1 < ast->arguments.size())
            Write(", ");
    }
    Write(")");
}

IMPLEMENT_VISIT_PROC(BracketExpr)
{
    Write("(");
    Visit(ast->expr);
    Write(")");
}

IMPLEMENT_VISIT_PROC(ObjectExpr)
{
    /* Write prefix expression (e.g. "a.b" -> visit a, then ".", then "b") */
    if (ast->prefixExpr && !ast->isStatic)
    {
        Visit(ast->prefixExpr);

        /* Add space between numeric literal prefix and .swizzle so '1.x' parses as '1 .x' not as a float literal */
        if (auto literalExpr = ast->prefixExpr->As<LiteralExpr>())
        {
            if (literalExpr->IsSpaceRequiredForSubscript())
                Write(" ");
        }

        Write(".");
    }
    else if (ast->prefixExpr && ast->isStatic)
    {
        Visit(ast->prefixExpr);
        Write("::");
    }

    /* Identifier */
    if (auto symbol = ast->symbolRef)
    {
        if (ast->flags(ObjectExpr::isImmutable))
            Write(symbol->ident.Original());
        else
            Write(symbol->ident);
    }
    else
    {
        Write(ast->ident);
    }
}

IMPLEMENT_VISIT_PROC(AssignExpr)
{
    Visit(ast->lvalueExpr);
    Write(" " + AssignOpToString(ast->op) + " ");
    Visit(ast->rvalueExpr);
}

IMPLEMENT_VISIT_PROC(ArrayExpr)
{
    Visit(ast->prefixExpr);
    for (auto& idx : ast->arrayIndices)
    {
        Write("[");
        Visit(idx);
        Write("]");
    }
}

IMPLEMENT_VISIT_PROC(CastExpr)
{
    /* HLSL C-style cast: (Type)expr */
    Write("(");
    if (ast->typeSpecifier && ast->typeSpecifier->typeDenoter)
        WriteTypeDenoter(*ast->typeSpecifier->typeDenoter, ast);
    Write(")");
    Visit(ast->expr);
}

IMPLEMENT_VISIT_PROC(InitializerExpr)
{
    Write("{ ");
    for (std::size_t i = 0; i < ast->exprs.size(); ++i)
    {
        Visit(ast->exprs[i]);
        if (i + 1 < ast->exprs.size())
            Write(", ");
    }
    Write(" }");
}

#undef IMPLEMENT_VISIT_PROC


/* --- Helper functions --- */

void HLSLGenerator::WriteTypeDenoter(const TypeDenoter& typeDenoter, const AST* ast)
{
    try
    {
        if (typeDenoter.IsVoid())
        {
            Write("void");
        }
        else if (auto baseTypeDen = typeDenoter.As<BaseTypeDenoter>())
        {
            /* DataTypeToString already produces HLSL syntax: "float4", "float4x4", etc. */
            Write(DataTypeToString(baseTypeDen->dataType));
        }
        else if (auto bufferTypeDen = typeDenoter.As<BufferTypeDenoter>())
        {
            auto bufferType = bufferTypeDen->bufferType;
            if (bufferType == BufferType::Undefined && bufferTypeDen->bufferDeclRef)
                bufferType = bufferTypeDen->bufferDeclRef->GetBufferType();

            Write(BufferTypeKeyword(bufferType));

            /* Optional generic sub-type: Texture2D<float4>, RWStructuredBuffer<S>, ... */
            if (bufferTypeDen->genericTypeDenoter)
            {
                Write("<");
                WriteTypeDenoter(*bufferTypeDen->genericTypeDenoter, ast);
                if (bufferTypeDen->genericSize > 1)
                {
                    Write(", ");
                    Write(std::to_string(bufferTypeDen->genericSize));
                }
                Write(">");
            }
        }
        else if (auto samplerTypeDen = typeDenoter.As<SamplerTypeDenoter>())
        {
            auto samplerType = samplerTypeDen->samplerType;
            if (samplerType == SamplerType::Undefined && samplerTypeDen->samplerDeclRef)
                samplerType = samplerTypeDen->samplerDeclRef->GetSamplerType();

            Write(SamplerTypeKeyword(samplerType));
        }
        else if (auto structTypeDen = typeDenoter.As<StructTypeDenoter>())
        {
            if (auto structDecl = structTypeDen->structDeclRef)
                Write(structDecl->ident);
            else
                Write(typeDenoter.Ident());
        }
        else if (typeDenoter.IsAlias())
        {
            WriteTypeDenoter(typeDenoter.GetAliased(), ast);
        }
        else if (auto arrayTypeDen = typeDenoter.As<ArrayTypeDenoter>())
        {
            /* For HLSL, array dimensions go on the *declarator*, not the type. But for
               cast-expressions and type-specifier-exprs we may emit the base type only;
               the variable visit will emit the dims. So just emit the sub type here. */
            if (arrayTypeDen->subTypeDenoter)
                WriteTypeDenoter(*arrayTypeDen->subTypeDenoter, ast);
        }
        else
        {
            Error(R_FailedToDetermineGLSLDataType, ast);
        }
    }
    catch (const Report&)
    {
        throw;
    }
    catch (const std::exception& e)
    {
        Error(e.what(), ast);
    }
}

void HLSLGenerator::WriteSemantic(const IndexedSemantic& semantic)
{
    if (!semantic.IsValid())
        return;

    Write(" : ");
    Write(SemanticToHLSL(semantic));
}

std::string HLSLGenerator::SemanticToHLSL(const IndexedSemantic& semantic) const
{
    if (semantic.IsUserDefined())
    {
        /* IndexedSemantic stores the prefix and the index separately when constructed from a string */
        std::string s = semantic.ToString();
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        return s;
    }

    /* System-value semantic: use the HLSL spelling and append the index */
    const auto& base = SemanticKeyword(static_cast<Semantic>(semantic));
    std::string s = base;
    if (semantic.Index() != 0)
        s += std::to_string(semantic.Index());
    return s;
}

void HLSLGenerator::WriteRegisters(const std::vector<RegisterPtr>& registers)
{
    if (registers.empty())
        return;

    /* Pick the register for the current shader target (or the unrestricted one) */
    auto* reg = Register::GetForTarget(registers, GetShaderTarget());
    if (!reg)
        return;

    Write(" : ");
    WriteRegister(*reg);
}

void HLSLGenerator::WriteRegister(const Register& reg)
{
    Write("register(");
    Write(std::string(1, RegisterTypeChar(reg.registerType)));
    Write(std::to_string(reg.slot));
    if (reg.space != 0)
    {
        Write(", space");
        Write(std::to_string(reg.space));
    }
    Write(")");
}

void HLSLGenerator::WriteAttributes(const std::vector<AttributePtr>& attribs)
{
    for (auto& attr : attribs)
    {
        if (attr)
            WriteAttribute(*attr);
    }
}

void HLSLGenerator::WriteAttribute(const Attribute& attrib)
{
    const auto& name = AttributeTypeKeyword(attrib.attributeType);
    if (name.empty())
        return;

    BeginLn();
    Write("[");
    Write(name);
    if (!attrib.arguments.empty())
    {
        Write("(");
        for (std::size_t i = 0; i < attrib.arguments.size(); ++i)
        {
            Visit(attrib.arguments[i]);
            if (i + 1 < attrib.arguments.size())
                Write(", ");
        }
        Write(")");
    }
    Write("]");
    EndLn();
}

void HLSLGenerator::WriteStorageClasses(const std::set<StorageClass>& storageClasses)
{
    for (auto sc : storageClasses)
    {
        const auto& kw = StorageClassKeyword(sc);
        if (!kw.empty())
            Write(kw + " ");
    }
}

void HLSLGenerator::WriteInterpModifiers(const std::set<InterpModifier>& interpModifiers)
{
    for (auto m : interpModifiers)
    {
        const auto& kw = InterpModifierKeyword(m);
        if (!kw.empty())
            Write(kw + " ");
    }
}

void HLSLGenerator::WriteTypeModifiers(const std::set<TypeModifier>& typeModifiers)
{
    for (auto m : typeModifiers)
    {
        const auto& kw = TypeModifierKeyword(m);
        if (!kw.empty())
            Write(kw + " ");
    }
}

void HLSLGenerator::WriteParameterModifiers(const TypeSpecifier& typeSpec)
{
    if (typeSpec.isInput && typeSpec.isOutput)
        Write("inout ");
    else if (typeSpec.isOutput)
        Write("out ");
    else if (typeSpec.isInput)
        Write("in ");
    if (typeSpec.isUniform)
        Write("uniform ");
}

void HLSLGenerator::WriteArrayDims(const std::vector<ArrayDimensionPtr>& arrayDims)
{
    for (auto& dim : arrayDims)
    {
        if (dim)
            Write(dim->ToString());
    }
}

void HLSLGenerator::WriteParameter(VarDeclStmnt* paramStmnt)
{
    /* Attributes are uncommon on parameters; emit if present */
    /* Interp modifiers and storage classes */
    WriteInterpModifiers(paramStmnt->typeSpecifier->interpModifiers);
    WriteStorageClasses(paramStmnt->typeSpecifier->storageClasses);

    /* Parameter direction modifiers */
    WriteParameterModifiers(*paramStmnt->typeSpecifier);

    /* Type modifiers */
    WriteTypeModifiers(paramStmnt->typeSpecifier->typeModifiers);

    /* Type */
    Visit(paramStmnt->typeSpecifier);
    Write(" ");

    if (paramStmnt->varDecls.size() == 1)
    {
        auto v = paramStmnt->varDecls.front().get();
        Write(v->ident);
        Visit(v->arrayDims);
        WriteSemantic(v->semantic);

        if (v->initializer)
        {
            Write(" = ");
            Visit(v->initializer);
        }
    }
}

void HLSLGenerator::WriteFunction(FunctionDecl* funcDecl)
{
    /* Attribute prefix (e.g. [numthreads(8,8,1)] for CS).
       FunctionDecl attributes live on the wrapping BasicDeclStmnt (Stmnt::attribs). */
    if (funcDecl->declStmntRef)
        WriteAttributes(funcDecl->declStmntRef->attribs);

    /* Return type + function name */
    BeginLn();

    if (funcDecl->returnType)
        Visit(funcDecl->returnType);
    Write(" ");
    Write(funcDecl->ident);

    /* Parameter list */
    Write("(");
    PushFunctionDecl(funcDecl);
    {
        for (std::size_t i = 0; i < funcDecl->parameters.size(); ++i)
        {
            WriteParameter(funcDecl->parameters[i].get());
            if (i + 1 < funcDecl->parameters.size())
                Write(", ");
        }
    }
    PopFunctionDecl();
    Write(")");

    /* Function semantic on the return value (e.g. ": SV_Position") */
    WriteSemantic(funcDecl->semantic);

    /* Function body or forward decl */
    if (funcDecl->codeBlock)
    {
        PushFunctionDecl(funcDecl);
        {
            Visit(funcDecl->codeBlock);
        }
        PopFunctionDecl();
    }
    else
    {
        Write(";");
        EndLn();
    }
}

void HLSLGenerator::WriteStructDecl(StructDecl* structDecl, bool endWithSemicolon)
{
    BeginLn();

    Write(structDecl->isClass ? "class" : "struct");
    if (!structDecl->ident.Empty())
        Write(" " + structDecl->ident);

    /* Inheritance (HLSL supports single inheritance for class/struct) */
    if (!structDecl->baseStructName.empty())
        Write(" : " + structDecl->baseStructName);

    WriteScopeOpen(false, endWithSemicolon);
    {
        Visit(structDecl->varMembers);

        /* Member functions */
        if (!structDecl->funcMembers.empty())
        {
            std::vector<BasicDeclStmnt*> funcMemberStmnts;
            funcMemberStmnts.reserve(structDecl->funcMembers.size());
            for (auto& funcDecl : structDecl->funcMembers)
                if (funcDecl->declStmntRef)
                    funcMemberStmnts.push_back(funcDecl->declStmntRef);
            for (auto* s : funcMemberStmnts)
                Visit(s);
        }
    }
    WriteScopeClose();

    if (!InsideVarDeclStmnt())
        Blank();
}

char HLSLGenerator::RegisterTypeChar(RegisterType type) const
{
    switch (type)
    {
        case RegisterType::ConstantBuffer:      return 'b';
        case RegisterType::TextureBuffer:       return 't';
        case RegisterType::BufferOffset:        return 'c';
        case RegisterType::Sampler:             return 's';
        case RegisterType::UnorderedAccessView: return 'u';
        default:                                return '?';
    }
}

template <typename T>
void HLSLGenerator::WriteStmntList(const std::vector<T>& stmnts, bool /*isGlobalScope*/)
{
    Visit(stmnts);
}

void HLSLGenerator::WriteScopedStmnt(Stmnt* ast)
{
    if (!ast)
        return;

    if (ast->Type() != AST::Types::CodeBlockStmnt)
    {
        WriteScopeOpen();
        {
            Visit(ast);
        }
        WriteScopeClose();
    }
    else
    {
        Visit(ast);
    }
}

bool HLSLGenerator::IsHidden(const AST* /*ast*/) const
{
    return false;
}


} // /namespace Xsc



// ================================================================================
