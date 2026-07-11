/*
 * OpaqueASTLowerer.h
 *
 * Phase 2 of the opaque-type lowering pass: functional AST rewriting.
 *
 * The lowerer consumes the finished OpaqueProgramPlan (read-only) and
 * rewrites the AST so that no opaque value remains in an illegal GLSL
 * position:
 *
 *  - Function signatures: opaque-bearing parameters lose their opaque lanes;
 *    each lane becomes its own synthesized scalar parameter (or a shared
 *    array parameter for dynamically indexed lane groups). Functions that
 *    returned opaque-bearing values become void; surviving residual data is
 *    returned through a synthesized 'out' parameter.
 *  - Call sites: opaque arguments are expanded into per-lane resource
 *    references; lowered-return calls are hoisted into prelude statements
 *    with a temporary receiving the residual.
 *  - Expressions/statements: reads of opaque values are replaced by direct
 *    references to the resources they are bound to; initializers are rebuilt
 *    to contain only their residual elements; evaluation order is preserved
 *    by hoisting earlier arguments into temporaries where needed.
 *  - Declarations: struct members and aliases that consisted purely of
 *    opaque data are stripped from the program.
 *
 * Statement lowering is functional in style: every statement lowers to a
 * list of statements (LowerStatement), and every expression lowers to a
 * LoweredExpression (a residual expression plus hoisted prelude statements).
 */

#ifndef XSC_OPAQUE_AST_LOWERER_H
#define XSC_OPAQUE_AST_LOWERER_H


#include "OpaqueProgramPlan.h"
#include "Visitor.h"
#include <cstddef>
#include <unordered_map>
#include <vector>


namespace Xsc
{

namespace OpaqueLowering
{


/* ----- Lowering result types ----- */

// Replacement declarations synthesized while rewriting one function's
// signature: for each original opaque-bearing formal parameter, the per-lane
// replacement parameters (indexed by lane; several lanes may share a single
// array parameter), plus the synthesized 'out' parameter that receives the
// residual part of a lowered return value.
struct SynthesizedSignature
{
    std::unordered_map<VarDecl*, std::vector<VarDecl*>> laneParameters;
    VarDecl*                                            returnResidual = nullptr;
};

// Lowering result of one expression: the surviving residual expression (may
// be null if the expression was purely opaque) plus any statements that must
// execute before it.
struct LoweredExpression
{
    ExprPtr                 residual;
    std::vector<StmntPtr>   prelude;
};


/* ----- Functional AST lowering ----- */

class OpaqueASTLowerer
{
    public:

        OpaqueASTLowerer(Program& program, OpaqueTypeLayoutCache& layouts, const OpaqueProgramPlan& plan);

        // Runs the whole lowering: rewrites all function signatures, then all
        // function bodies, then strips fully-opaque type declarations.
        void Lower();

    private:

        // A call/initializer argument that must be hoisted into a temporary
        // if a later argument produces prelude statements (preserving the
        // original left-to-right evaluation order). Arrays and opaque types
        // are never hoisted.
        struct PendingArgument
        {
            std::size_t     argumentIndex = 0;
            TypeDenoterPtr  type;
        };

        /* ----- Signature rewriting ----- */

        // Rewrites every function's parameter list and return type according
        // to its FunctionABI, recording the synthesized declarations in
        // synthesizedSignatures_.
        void RewriteSignatures();

        /* ----- Statement lowering ----- */

        // Lowers every function body in the program.
        void RewriteBodies();

        // Rewrites a statement list in place by flat-mapping LowerStatement.
        void LowerStatementList(std::vector<StmntPtr>& statements);

        // Lowers one statement into its replacement statement list. Rejects
        // hoisted preludes in positions that would change their execution
        // frequency (loop conditions/iterations, case labels) with
        // R_OpaqueTypeInvalidRuntimeIndex.
        std::vector<StmntPtr> LowerStatement(const StmntPtr& statement);

        // Lowers a variable declaration statement; declarations of purely
        // opaque variables are dropped entirely.
        std::vector<StmntPtr> LowerVariableDeclaration(const StmntPtr& statement, VarDeclStmnt& declaration);

        // Lowers a bare expression statement; opaque-only assignments reduce
        // to just the rvalue's prelude.
        std::vector<StmntPtr> LowerExpressionStatement(const StmntPtr& statement, ExprStmnt& exprStmnt);

        // Lowers a return statement; when the function's return type was
        // lowered, the residual value is assigned to the synthesized out
        // parameter and the return becomes value-less.
        std::vector<StmntPtr> LowerReturn(const StmntPtr& statement, ReturnStmnt& ret);

        // Wraps LowerStatement's result into a single statement suitable as
        // a loop/if body (null statement, the single statement, or a block).
        StmntPtr LowerEmbedded(const StmntPtr& statement);

        /* ----- Expression lowering ----- */

        // Lowers one expression. 'expectedLayout'/'expectedNode' provide the
        // declared shape when lowering initializer contexts.
        LoweredExpression LowerExpression(
            const ExprPtr& expression,
            const OpaqueTypeLayoutPtr& expectedLayout,
            const LayoutNodePtr& expectedNode
        );

        // Lowers an initializer list; with an opaque-bearing expected layout
        // the residual structure is rebuilt via BuildResidualInitializer.
        LoweredExpression LowerInitializer(
            const ExprPtr& expression,
            InitializerExpr& initializer,
            const OpaqueTypeLayoutPtr& layout,
            const LayoutNodePtr& node
        );

        // Recursively rebuilds the residual part of an initializer from the
        // flat element list, using the layout's source-slot numbering to
        // locate each leaf. Returns null for nodes without residual data.
        ExprPtr BuildResidualInitializer(
            const LayoutNodePtr& node,
            const std::vector<ExprPtr>& elements,
            std::size_t sourceBase,
            std::vector<StmntPtr>& prelude
        );

        // Rewrites a call according to its CallSitePlan: expands opaque
        // arguments into per-lane resource references and, for lowered
        // returns, hoists the call into the prelude with a residual-result
        // temporary.
        LoweredExpression LowerCall(const ExprPtr& expression, CallExpr& call, const CallSitePlan& callPlan);

        // Lowers a ternary; if either branch needs prelude statements, the
        // ternary is rewritten into an if/else assigning a temporary.
        LoweredExpression LowerTernary(
            const ExprPtr& expression,
            TernaryExpr& ternary,
            const OpaqueTypeLayoutPtr& expectedLayout,
            const LayoutNodePtr& expectedNode
        );

        // Lowers a binary expression; short-circuit operators whose rhs needs
        // prelude statements are rewritten into an if statement over a bool
        // temporary to preserve short-circuit semantics.
        LoweredExpression LowerBinary(const ExprPtr& expression, BinaryExpr& binary);

        // If the analysis recorded a purely-opaque value for 'expression',
        // replaces the residual with a direct resource reference (or drops
        // it when the value spans multiple lanes).
        LoweredExpression ReplaceOpaqueValue(const ExprPtr& expression, LoweredExpression result);

        // Collapses the bindings of an opaque array lane group into a single
        // expression referencing one cohesive resource array (or one
        // synthesized formal array parameter). Raises
        // R_OpaqueTypeInvalidRuntimeIndex if the bindings do not form one
        // contiguous slice of a single resource.
        ExprPtr CollapseArrayLane(
            const OpaqueValue& value,
            const std::vector<std::size_t>& group,
            const AST* context
        );

        // Converts one OpaqueBinding into the expression that references its
        // resource or synthesized lane parameter. Raises
        // R_OpaqueStructUninitialized / R_OpaqueStructAmbiguousAlias /
        // R_OpaqueTypeInvalidRuntimeIndex for unreadable bindings.
        ExprPtr BindingExpression(const OpaqueBinding& binding, const AST* context);

        // Registers argument 'argumentIndex' as hoistable (skipped for null,
        // array, and opaque types).
        static void AddPendingArgument(
            std::vector<PendingArgument>& pending,
            std::size_t argumentIndex,
            const TypeDenoterPtr& type
        );

        // Materializes every pending argument into a prelude temporary and
        // rewrites the argument list to reference the temporaries.
        void FlushPendingArguments(
            std::vector<ExprPtr>& arguments,
            std::vector<PendingArgument>& pending,
            std::vector<StmntPtr>& prelude
        );

        /* ----- Declaration stripping ----- */

        // Removes global struct/alias declarations that consisted purely of
        // opaque data, and strips fully-opaque members from surviving structs.
        void StripTypeDeclarations();

        // Removes fully-opaque member variables from one struct declaration.
        void StripStructMembers(StructDecl& structDecl);

        /* ----- Small AST construction helpers ----- */

        // Wraps a statement list into a code block statement.
        static StmntPtr MakeBlock(std::vector<StmntPtr> statements);

        // Returns a single-element statement list.
        static std::vector<StmntPtr> SingleStatement(const StmntPtr& statement);

        // Appends 'source' to 'destination'.
        static void Append(std::vector<StmntPtr>& destination, const std::vector<StmntPtr>& source);

    private:

        Program&                                                program_;
        OpaqueTypeLayoutCache&                                  layouts_;
        const OpaqueProgramPlan&                                plan_;
        std::unordered_map<FunctionDecl*, SynthesizedSignature> synthesizedSignatures_;
        FunctionDecl*                                           activeFunction_ = nullptr;
        std::size_t                                             nextTempId_ = 0;    // Counter for unique temporary names.
};


} // /namespace OpaqueLowering

} // /namespace Xsc


#endif



// ================================================================================
