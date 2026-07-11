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
 *      its own synthesized parameter (one per flattened path). Pure 'out' parameters
 *      are not split (the callee cannot read their opaque leaves before writing
 *      them, so the caller has nothing to pass in); 'inout' parameters are split
 *      like by-value ones.
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
 *   6. Propagates alias resolution across call boundaries. While a body is
 *      processed, a per-function summary records how the opaque leaves of its
 *      return value (joined over all return statements) and of each out/inout
 *      opaque-struct parameter (joined over all function exits) resolve, in
 *      callee-context targets (globals, or the callee's own opaque parameters).
 *      Every call site translates that summary into the caller by mapping parameter
 *      targets to the resolved arguments it passed, enabling
 *      `Bundle b = make();`, `dst = make();`, `return make();`, direct pass-through
 *      (`helper(make(), uv)`), ternaries over struct values, and out/inout
 *      copy-back after the call. Callee bodies are processed on demand (memoized),
 *      so a call site never reads a missing summary.
 *
 * Restrictions enforced by the frontend (HLSLAnalyzer) before this pass runs:
 *   - opaque-bearing structs may not appear as globals, cbuffer/SB members,
 *     entry-point I/O, member-function carriers, or as arrays (including arrays as
 *     parameters or return types).
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

        // One synthesized opaque parameter: the VarDecl appended to the signature and the
        // dotted opaque field path of the original struct it carries.
        struct OpaqueParam
        {
            VarDecl*    param = nullptr;
            std::string field;
        };

        // For each function whose params include opaque-bearing struct types,
        // remembers the synthesized opaque-field parameters that were appended after
        // each original parameter, in declaration order.
        struct FunctionRewriteInfo
        {
            // The number of parameters before any rewriting (i.e. the original count).
            // This lets us iterate over the original parameter slots safely after
            // the parameter list has been augmented.
            std::size_t originalParamCount = 0;

            // Per-original-parameter index -> ordered list of synthesized opaque params
            // (VarDecl + field path). Empty entry means that parameter was not opaque-bearing.
            std::vector<std::vector<OpaqueParam>> opaqueParamsPerOriginal;
        };

        // Alias state for a single opaque field of a local opaque-bearing struct var.
        struct AliasEntry
        {
            // Tri-state resolution of the field at the current program point.
            enum class State
            {
                Unset,      // seeded but not yet bound to a global
                Resolved,   // bound to `target`
                Ambiguous,  // a control-flow join made it unresolvable
            };

            State       state       = State::Unset;
            // Resolved global declaration (meaningful only when state == Resolved). May be
            // a global BufferDecl/SamplerDecl, or a new opaque parameter VarDecl introduced
            // by signature rewriting. We use the common Decl base so all three can be stored
            // uniformly.
            Decl*       target      = nullptr;

            static AliasEntry MakeResolved(Decl* target)
            {
                AliasEntry e;
                e.state  = State::Resolved;
                e.target = target;
                return e;
            }
            static AliasEntry MakeAmbiguous()
            {
                AliasEntry e;
                e.state = State::Ambiguous;
                return e;
            }
        };

        using AliasMap = std::unordered_map<std::string, AliasEntry>;

        // Per-variable alias state: the alias map of every opaque-bearing struct local
        // currently in scope. Snapshotted and joined at control-flow merge points.
        using AliasState = std::unordered_map<VarDecl*, AliasMap>;

        // One entry per slot of a flat aggregate initializer, in struct-layout order.
        struct InitSlot
        {
            enum class Kind
            {
                Opaque,             // an opaque resource leaf (dropped from the residual)
                Pod,                // a POD leaf (kept in the residual)
                EmptyNestedDummy,   // a fully-opaque nested struct (becomes one dummy int)
            };
            Kind        kind        = Kind::Pod;
            std::string path;                   // dotted path (Opaque)
            std::size_t exprIndex   = 0;        // initializer slot read (Opaque/Pod)
            // For EmptyNestedDummy: the inner opaque leaves (dotted path, initializer slot)
            // whose aliases must still be seeded even though the residual collapses to one
            // dummy int.
            std::vector<std::pair<std::string, std::size_t>> innerOpaqueLeaves;
        };

        // Dataflow summary of one function, computed while its body is processed: how
        // the opaque leaves of its return value and of each out/inout opaque-struct
        // parameter resolve at function exit. Targets are callee-context Decls (global
        // BufferDecl/SamplerDecl, or one of the callee's own opaque parameter VarDecls);
        // call sites translate them into caller context via the argument bindings.
        struct FunctionSummary
        {
            // True once at least one ReturnStmnt of an opaque-struct-returning function
            // has been summarized (multiple returns are joined; differing bindings
            // become ambiguous).
            bool     hasReturnAliases = false;
            AliasMap returnAliases;

            // Exit-state alias map of each out/inout opaque-struct parameter (keyed by
            // the parameter's VarDecl), joined over every return statement and the
            // fall-through end of the body.
            std::unordered_map<VarDecl*, AliasMap> outParamAliases;
        };

        /* ----- Pass orchestration ----- */

        void RewriteFunctionSignatures(Program& program);
        void RewriteFunctionBodies(Program& program);
        void StripOpaqueMembersFromStructs(Program& program);

        /* ----- Helpers ----- */

        // If the type denoter resolves to a struct decl that HasOpaqueMember, returns
        // that StructDecl; otherwise returns null.
        static StructDecl* TryGetOpaqueStructDeclaration(const TypeDenoterPtr& typeDen);

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

        // Decomposes any expression denoting a (whole or sub-) opaque-bearing struct value
        // -- a bare local, a sub-struct field chain, optionally parenthesized -- into its
        // base local and the dotted sub-path within that local's alias map ("" when the
        // whole variable is referenced). Returns false if it does not reference a tracked
        // local. The single entry point for arguments, initializers and assignment sides;
        // ResolveFieldChain is its recursive worker for the field-chain case.
        bool DecomposeToVarPath(Expr* expr, VarDecl*& outVar, std::string& outPath);

        // Walks the flat aggregate-initializer slots of `structDecl` (base members first,
        // then declared members, nested opaque-bearing structs flattened in place), up to
        // `exprCount` available initializer expressions, producing one InitSlot per slot.
        // The single source of truth for the initializer's member/index layout, shared by
        // the alias-seeding and residual-stripping walks.
        void BuildInitializerSlots(StructDecl* structDecl, std::size_t exprCount, std::vector<InitSlot>& outSlots);

        // Initializes the alias map for a newly-declared local from its initializer expression.
        void InitAliasFromInitializer(VarDecl* localVar, StructDecl* structDecl, Expr* initializer);

        /* ----- Cross-call propagation ----- */

        // Processes one function body exactly once (memoized in processedFuncs_):
        // seeds parameter alias maps, walks the body (applying all rewrites), and
        // captures the function's return-value / out-param summaries. Alias state and
        // the struct-decl tracker stack are saved and restored around the body, so a
        // callee processed on demand from the middle of a caller's body neither sees
        // nor pollutes the caller's state. Forward declarations (no body) are marked
        // processed and skipped.
        void ProcessFunction(FunctionDecl* funcDecl);

        // Folds the CURRENT alias state of every out/inout opaque-struct parameter of
        // `funcDecl` into its exit summary (first capture copies, later captures join).
        // Called at every ReturnStmnt and once more at the fall-through end of the body.
        void AccumulateOutParamState(FunctionDecl* funcDecl);

        // Computes the alias map (keys relative to `structDecl`'s opaque leaves) of an
        // expression that yields an opaque-bearing struct value:
        //   - a (whole or sub-) reference to a tracked local/parameter,
        //   - a call whose translated return aliases were recorded (expr must already
        //     have been visited),
        //   - a ternary over two such values (sides are joined),
        //   - any of the above in brackets.
        // Returns false if the expression's aliases cannot be determined.
        bool GetOrComputeAliasMapForExpression(Expr* expr, StructDecl* structDecl, AliasMap& outMap);

        // Translates alias map built by callee for its return values and output parameters. 
        // Maps those aliases into caller's variables (or global variables) through @p paramBindings 
        // (set of arguments caller passed to the callee).
        // 
        static AliasMap RemapAliasesCalleeToCaller(const AliasMap& calleeMap, const std::unordered_map<VarDecl*, Decl*>& paramBindings);

        // Writes every entry of `srcMap` (keys relative to some struct type) into
        // `destMap` under `destPath` ("" = the whole variable), overwriting the
        // previous entries for those leaves.
        static void RemapAliasMap(AliasMap& destMap, const std::string& destPath, const AliasMap& srcMap);

        // Visit overrides used by RewriteFunctionBodies.
        DECL_VISIT_PROC( StructDecl        );
        DECL_VISIT_PROC( FunctionDecl      );
        DECL_VISIT_PROC( VarDeclStmnt      );
        DECL_VISIT_PROC( CallExpr          );
        DECL_VISIT_PROC( ObjectExpr        );
        DECL_VISIT_PROC( AssignExpr        );
        DECL_VISIT_PROC( ExprStmnt         );
        DECL_VISIT_PROC( ReturnStmnt       );
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

        // Joins `src` into `dst` per variable, for keys present in both (their alias maps
        // are merged with JoinAliasMaps). Variables only in `src` are left untouched.
        static void JoinCommonInto(AliasState& dst, const AliasState& src);

        // Like JoinCommonInto, but also carries over variables that exist only in `src`
        // (e.g. a local first bound inside one branch of an if/else).
        static void JoinStateInto(AliasState& dst, const AliasState& src);

        // Copies the alias entries for all opaque leaves under `srcPath` in `srcMap` into
        // `destMap` under `destPath` (either path "" means the whole struct). Used for
        // whole-struct and sub-struct copy assignments (`dst = src;`, `dst.sub = src;`).
        static void CopyAliasSubtree(AliasMap& destMap, const std::string& destPath,
            const AliasMap& srcMap, const std::string& srcPath);

        /* ----- Members ----- */

        NameMangling                                    nameMangling_;
        std::unordered_map<FunctionDecl*, FunctionRewriteInfo>  funcRewrites_;
        AliasState                                      activeAliasMaps_;

        // Per-function return/out-param summaries (keyed by the implementation's
        // FunctionDecl), produced by ProcessFunction.
        std::unordered_map<FunctionDecl*, FunctionSummary>  funcSummaries_;

        // Per-call-site translated (caller-context) return aliases of calls to
        // functions that return an opaque-bearing struct.
        std::unordered_map<CallExpr*, AliasMap>             callReturnAliases_;

        // Function bodies already processed (or currently being processed); makes
        // ProcessFunction idempotent and breaks (illegal) recursion cycles.
        std::unordered_set<FunctionDecl*>                   processedFuncs_;

};


} // /namespace Xsc


#endif



// ================================================================================
