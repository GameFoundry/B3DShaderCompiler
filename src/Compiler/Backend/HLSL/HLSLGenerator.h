/*
 * HLSLGenerator.h
 *
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#ifndef XSC_HLSL_GENERATOR_H
#define XSC_HLSL_GENERATOR_H


#include <Xsc/Xsc.h>
#include "AST.h"
#include "Generator.h"
#include "Visitor.h"
#include "Token.h"
#include "ASTEnums.h"


namespace Xsc
{


struct TypeDenoter;
struct BaseTypeDenoter;

// HLSL output code generator. Emits HLSL syntax from the (HLSL-flavored) AST,
// effectively performing a round-trip HLSL -> AST -> HLSL pretty-printing pass.
//
// Designed to be derivable: helper methods and per-token keyword lookups are
// protected virtuals so that backends emitting an HLSL-flavored dialect 
// can override only the divergent pieces.
class HLSLGenerator : public Generator
{

    public:

        HLSLGenerator(Log* log);

    protected:

        /* === Generator override === */

        void GenerateCodePrimary(
            Program& program,
            const ShaderInput& inputDesc,
            const ShaderOutput& outputDesc
        ) override;

        /* === File-header hook (overridable) === */

        // Emits the file-level header comment (generator credit + timestamp).
        virtual void WriteFileHeader(const ShaderInput& inputDesc);

        /* === Token-spelling virtuals (overridable for HLSL-like dialects) === */

        // HLSL spelling for a system-value semantic enum (e.g. "SV_Position").
        virtual const std::string& SemanticKeyword(Semantic s) const;

        // HLSL spelling for an attribute enum (e.g. "numthreads", "unroll").
        virtual const std::string& AttributeTypeKeyword(AttributeType type) const;

        // HLSL spelling for a storage-class enum (e.g. "static", "groupshared").
        virtual const std::string& StorageClassKeyword(StorageClass s) const;

        // HLSL spelling for an interpolation-modifier enum (e.g. "linear", "centroid").
        virtual const std::string& InterpModifierKeyword(InterpModifier m) const;

        // HLSL spelling for a type-modifier enum (e.g. "const", "row_major").
        virtual const std::string& TypeModifierKeyword(TypeModifier m) const;

        // HLSL spelling for a sampler-type enum (e.g. "SamplerState").
        virtual const std::string& SamplerTypeKeyword(SamplerType t) const;

        // HLSL spelling for a buffer-type enum (e.g. "StructuredBuffer", "RWTexture2D").
        // Returns empty string for invalid/unknown types. Returned by value because
        // BufferTypeToString in ASTEnums.cpp returns by value.
        virtual std::string BufferTypeKeyword(BufferType t) const;

        /* === HLSL-specific helpers (overridable) === */

        // Writes type denoter using HLSL syntax (e.g. "float4", "Texture2D<float4>", "RWStructuredBuffer<S>").
        virtual void WriteTypeDenoter(const TypeDenoter& typeDenoter, const AST* ast = nullptr);

        // Writes the optional semantic suffix (e.g. " : SV_Position", " : TEXCOORD0").
        virtual void WriteSemantic(const IndexedSemantic& semantic);

        // Writes the original HLSL identifier for a system-value semantic or the user-defined name.
        virtual std::string SemanticToHLSL(const IndexedSemantic& semantic) const;

        // Writes the register annotation list (e.g. " : register(t0, space1)").
        virtual void WriteRegisters(const std::vector<RegisterPtr>& registers);

        // Writes a single register annotation.
        virtual void WriteRegister(const Register& reg);

        // Writes any attached attributes preceding a declaration (e.g. "[unroll]", "[numthreads(8,8,1)]").
        virtual void WriteAttributes(const std::vector<AttributePtr>& attribs);

        // Writes one attribute (e.g. "[unroll]", "[branch]", "[numthreads(8,8,1)]").
        virtual void WriteAttribute(const Attribute& attrib);

        // Writes storage classes (static, extern, ...).
        virtual void WriteStorageClasses(const std::set<StorageClass>& storageClasses);

        // Writes interpolation modifiers (linear, centroid, ...).
        virtual void WriteInterpModifiers(const std::set<InterpModifier>& interpModifiers);

        // Writes type modifiers (const, row_major, column_major, snorm, unorm).
        virtual void WriteTypeModifiers(const std::set<TypeModifier>& typeModifiers);

        // Writes the input modifier prefix ("in", "out", "inout", "uniform").
        virtual void WriteParameterModifiers(const TypeSpecifier& typeSpec);

        // Writes an array-dimension suffix (e.g. "[4][2]").
        virtual void WriteArrayDims(const std::vector<ArrayDimensionPtr>& arrayDims);

        // Writes the body of a function parameter list.
        virtual void WriteParameter(VarDeclStmnt* paramStmnt);

        // Writes a function declaration.
        virtual void WriteFunction(FunctionDecl* funcDecl);

        // Writes a struct declaration (without enclosing statement).
        virtual void WriteStructDecl(StructDecl* structDecl, bool endWithSemicolon);

        // Returns the HLSL register-type character ('b', 't', 's', 'u', 'c').
        virtual char RegisterTypeChar(RegisterType type) const;

        // Writes a single statement; wraps non-block bodies in a block when needed for readability.
        virtual void WriteScopedStmnt(Stmnt* ast);

        /* --- Visit procs derived backends override --- */

        DECL_VISIT_PROC( UniformBufferDecl );
        DECL_VISIT_PROC( CallExpr          );

    private:

        /* --- Visitor implementation (private — derived backends should not need to override) --- */

        DECL_VISIT_PROC( Program           );
        DECL_VISIT_PROC( CodeBlock         );
        DECL_VISIT_PROC( SwitchCase        );
        DECL_VISIT_PROC( ArrayDimension    );
        DECL_VISIT_PROC( TypeSpecifier     );

        DECL_VISIT_PROC( VarDecl           );
        DECL_VISIT_PROC( StructDecl        );
        DECL_VISIT_PROC( SamplerDecl       );

        DECL_VISIT_PROC( FunctionDecl      );
        DECL_VISIT_PROC( BufferDeclStmnt   );
        DECL_VISIT_PROC( SamplerDeclStmnt  );
        DECL_VISIT_PROC( VarDeclStmnt      );
        DECL_VISIT_PROC( AliasDeclStmnt    );
        DECL_VISIT_PROC( BasicDeclStmnt    );

        DECL_VISIT_PROC( NullStmnt         );
        DECL_VISIT_PROC( CodeBlockStmnt    );
        DECL_VISIT_PROC( ForLoopStmnt      );
        DECL_VISIT_PROC( WhileLoopStmnt    );
        DECL_VISIT_PROC( DoWhileLoopStmnt  );
        DECL_VISIT_PROC( IfStmnt           );
        DECL_VISIT_PROC( ElseStmnt         );
        DECL_VISIT_PROC( SwitchStmnt       );
        DECL_VISIT_PROC( ExprStmnt         );
        DECL_VISIT_PROC( ReturnStmnt       );
        DECL_VISIT_PROC( CtrlTransferStmnt );

        DECL_VISIT_PROC( SequenceExpr      );
        DECL_VISIT_PROC( LiteralExpr       );
        DECL_VISIT_PROC( TypeSpecifierExpr );
        DECL_VISIT_PROC( TernaryExpr       );
        DECL_VISIT_PROC( BinaryExpr        );
        DECL_VISIT_PROC( UnaryExpr         );
        DECL_VISIT_PROC( PostUnaryExpr     );
        DECL_VISIT_PROC( BracketExpr       );
        DECL_VISIT_PROC( ObjectExpr        );
        DECL_VISIT_PROC( AssignExpr        );
        DECL_VISIT_PROC( ArrayExpr         );
        DECL_VISIT_PROC( CastExpr          );
        DECL_VISIT_PROC( InitializerExpr   );

        // Helper: writes an HLSL stmnt list (delegating to per-stmnt visitor) with optional separator blanks.
        template <typename T>
        void WriteStmntList(const std::vector<T>& stmnts, bool isGlobalScope = false);

        // Returns whether the given AST node is a hidden "compiler-built" node (e.g. dummy entry-points).
        // Hidden nodes should be skipped during HLSL emission to keep round-trip output close to the input.
        bool IsHidden(const AST* ast) const;

};


} // /namespace Xsc


#endif



// ================================================================================
