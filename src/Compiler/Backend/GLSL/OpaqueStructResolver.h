/*
 * OpaqueStructResolver.h
 *
 * Pre-emission AST transformation that allows HLSL source to declare structs
 * containing opaque resources (Texture2D, SamplerState, Buffer<>, etc.) and pass
 * them through function calls, while emitting valid GLSL/SPIR-V.
 *
 * Opaque members may be nested inside further opaque-bearing struct members; such
 * fields are flattened to dotted access paths (e.g. "mat.albedo") throughout.
 *
 * GLSL/SPIR-V disallow opaque types as struct members, so this pass:
 *   1. Walks function declarations: for parameters whose type is a struct that
 *      contains opaque members (directly or nested), splits each opaque field out as
 *      its own synthesized parameter (one per flattened path).
 *   2. Walks function bodies: tracks, for every local variable of opaque-bearing
 *      struct type, which global resource each opaque field (by dotted path)
 *      currently aliases. Straight-line initializers, copies (whole or sub-struct)
 *      and field assignments are supported; conditional rebinding causes the field to
 *      become "ambiguous" at the join point and is rejected if subsequently read.
 *   3. Rewrites every `s.[a.b...].opaqueField` ObjectExpr chain that resolves through
 *      the alias map (or new opaque parameter) to reference the global / parameter
 *      directly.
 *   4. Rewrites every CallExpr that passes an opaque-bearing struct argument:
 *      the original argument expression is followed by the resolved opaque
 *      arguments (one per opaque field).
 *   5. Strips opaque members from StructDecl::varMembers so the generator emits
 *      only the POD residual.
 *
 * Restrictions enforced by the frontend (HLSLAnalyzer) before this pass runs:
 *   - opaque-bearing structs may not appear as globals, cbuffer/SB members,
 *     entry-point I/O, return-by-value, member-function carriers, or as arrays.
 */

#ifndef XSC_OPAQUE_STRUCT_RESOLVER_H
#define XSC_OPAQUE_STRUCT_RESOLVER_H


#include "VisitorTracker.h"
#include "TypeDenoter.h"
#include <Xsc/Xsc.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>


namespace Xsc
{


class OpaqueStructResolver : public VisitorTracker
{

    public:

        void Resolve(Program& program, const NameMangling& nameMangling);

    private:

        /* ----- Internal data ----- */

        // For each function whose params include opaque-bearing struct types,
        // remembers the synthesized opaque-field parameter VarDecls that were
        // appended after each original parameter, in declaration order.
        struct FunctionRewriteInfo
        {
            // The number of parameters before any rewriting (i.e. the original count).
            // This lets us iterate over the original parameter slots safely after
            // the parameter list has been augmented.
            std::size_t originalParamCount = 0;

            // Per-original-parameter index -> ordered list of new opaque parameter VarDecls.
            // Empty entry means that parameter was not opaque-bearing.
            std::vector<std::vector<VarDecl*>> opaqueParamsPerOriginal;

            // Per-original-parameter index -> ordered list of opaque field names
            // (parallel to opaqueParamsPerOriginal). Used to decompose call arguments.
            std::vector<std::vector<std::string>> opaqueFieldsPerOriginal;
        };

        // Alias state for a single opaque field of a local opaque-bearing struct var.
        struct AliasEntry
        {
            // Resolved global declaration. May be a global BufferDecl/SamplerDecl, or a
            // new opaque parameter VarDecl introduced by signature rewriting. We use the
            // common Decl base so all three can be stored uniformly.
            Decl*       target      = nullptr;
            bool        ambiguous   = false;    // Set if a control-flow join made it unresolvable.
        };

        using AliasMap = std::unordered_map<std::string, AliasEntry>;

        /* ----- Pass orchestration ----- */

        void RewriteFunctionSignatures(Program& program);
        void RewriteFunctionBodies(Program& program);
        void StripOpaqueMembersFromStructs(Program& program);

        /* ----- Helpers ----- */

        // True if the type denoter resolves to a struct decl that HasOpaqueMember.
        // Out parameter receives the StructDecl, if any.
        static StructDecl* ResolveOpaqueStruct(const TypeDenoterPtr& typeDen);

        // Collects (in declaration order) the opaque members of a struct, including
        // inherited members and members nested inside opaque-bearing sub-structs.
        // Returns pairs of (dotted access path, type denoter copy); e.g. a struct with
        // member `Material mat; Texture2D tex;` yields "mat.albedo", "mat.samp", "tex".
        // `prefix` is prepended to every collected path (used during recursion).
        static void CollectOpaqueFields(StructDecl* structDecl, std::vector<std::pair<std::string, TypeDenoterPtr>>& outFields, const std::string& prefix = ""
        );

        // True if every leaf member of the struct (recursing through base structs and
        // nested opaque-bearing struct members) is an opaque resource, i.e. nothing
        // survives stripping. Such a struct becomes empty and the generator gives it a
        // dummy member.
        static bool StructIsFullyOpaque(StructDecl* structDecl);

        // Splits one parameter VarDeclStmnt of opaque-bearing struct type by
        // appending new opaque-only parameters. `actualIndex` is the parameter's
        // current position in funcDecl.parameters; `logicalIndex` is its position
        // in the function's original parameter list (used as the index into info).
        void SplitOpaqueParameter(FunctionDecl& funcDecl, std::size_t actualIndex, std::size_t logicalIndex, FunctionRewriteInfo& info);

        // For a body access `prefix.field` where `prefix` is a local opaque-bearing
        // struct variable, returns the Decl that should replace the whole ObjectExpr
        // (i.e. the resolved global or new opaque param). Returns null and reports an
        // error if the alias is ambiguous or unset. Returns null without error if the
        // access is not on an opaque field (caller should leave it alone).
        Decl* ResolveOpaqueFieldAccess(VarDecl* localVar, const std::string& fieldName, const AST* errorContext);

        // Tries to read alias info for a local var.
        AliasMap* FindAliasMap(VarDecl* localVar);
        const AliasMap* FindAliasMap(VarDecl* localVar) const;

        // Decomposes a member-access chain `localVar.f1.f2...fN` (an ObjectExpr) rooted
        // at a tracked local/parameter into that local and the dotted field path
        // "f1.f2...fN". Returns false if the expression is not such a chain.
        bool ResolveFieldChain(ObjectExpr* obj, VarDecl*& outVar, std::string& outPath);

        // Resolves an argument/initializer expression that denotes a (whole or sub-)
        // opaque-bearing struct value to its base local and the dotted sub-path within
        // that local's alias map ("" when the whole variable is referenced). Returns
        // false if the expression does not reference a tracked local.
        bool ResolveArgToVarPath(Expr* expr, VarDecl*& outVar, std::string& outPath);

        // Initializes the alias map for a newly-declared local from its initializer expression.
        void InitAliasFromInitializer(VarDecl* localVar, StructDecl* structDecl, Expr* initializer);

        // Visit overrides used by RewriteFunctionBodies.
        DECL_VISIT_PROC( StructDecl        );
        DECL_VISIT_PROC( FunctionDecl      );
        DECL_VISIT_PROC( VarDeclStmnt      );
        DECL_VISIT_PROC( CallExpr          );
        DECL_VISIT_PROC( ObjectExpr        );
        DECL_VISIT_PROC( AssignExpr        );
        DECL_VISIT_PROC( ExprStmnt         );
        DECL_VISIT_PROC( IfStmnt           );
        DECL_VISIT_PROC( ElseStmnt         );
        DECL_VISIT_PROC( ForLoopStmnt      );
        DECL_VISIT_PROC( WhileLoopStmnt    );
        DECL_VISIT_PROC( DoWhileLoopStmnt  );
        DECL_VISIT_PROC( SwitchStmnt       );
        DECL_VISIT_PROC( CodeBlock         );

        // Joins two alias maps coming out of two control-flow branches into a single
        // map; any field that differs becomes ambiguous.
        static AliasMap JoinAliasMaps(const AliasMap& a, const AliasMap& b);

        // Copies the alias entries for all opaque leaves under `srcPath` in `srcMap` into
        // `destMap` under `destPath` (either path "" means the whole struct). Used for
        // whole-struct and sub-struct copy assignments (`dst = src;`, `dst.sub = src;`).
        static void CopyAliasSubtree(AliasMap& destMap, const std::string& destPath,
            const AliasMap& srcMap, const std::string& srcPath);

        /* ----- Members ----- */

        NameMangling                                    nameMangling_;
        std::unordered_map<FunctionDecl*, FunctionRewriteInfo>  funcRewrites_;
        std::unordered_map<VarDecl*, AliasMap>          activeAliasMaps_;

};


} // /namespace Xsc


#endif



// ================================================================================
