/*
 * OpaqueProgramAnalyzer.h
 *
 * Phase 1 of the opaque-type lowering pass: symbolic dataflow analysis.
 *
 * The analyzer never modifies the AST. It builds per-function contracts and
 * ABIs, then interprets each function body abstractly: for every value that
 * contains opaque lanes it tracks an OpaqueValue (one OpaqueBinding per
 * lane) through assignments, initializers, calls, and control flow. Merge
 * points join bindings lane-wise; disagreeing lanes become Conflict and are
 * only an error if actually read. All results — expression values, call-site
 * plans, dynamic lane groups, variable layouts — are recorded into the
 * shared OpaqueProgramPlan for the lowering phase.
 *
 * Functions are analyzed on demand (callees before their call sites) with
 * memoized FunctionSummary results; recursion is rejected.
 */

#ifndef XSC_OPAQUE_PROGRAM_ANALYZER_H
#define XSC_OPAQUE_PROGRAM_ANALYZER_H


#include "OpaqueProgramPlan.h"
#include "Visitor.h"
#include <cstddef>
#include <vector>


namespace Xsc
{

namespace OpaqueLowering
{


/* ----- Access descriptions ----- */

// One non-constant array subscript within an otherwise constant access path,
// remembered by its position in the path.
struct DynamicStep
{
    std::size_t position = 0;
    ExprPtr     expression;
};

// A parsed lvalue/rvalue access chain: the root variable, the constant path
// (dynamic subscripts appear as placeholder index 0), and the dynamic steps.
struct AccessDescription
{
    VarDecl*                 root = nullptr;
    AccessPath               constantPath;
    std::vector<DynamicStep> dynamicSteps;
};


/* ----- Dataflow results ----- */

// Result of abstractly executing a statement (or statement list): the state
// on normal fall-through plus the states captured at every abnormal exit.
struct FlowResult
{
    bool                            fallsThrough = true;
    BindingEnvironment              fallthrough;
    std::vector<BindingEnvironment> returns;
    std::vector<BindingEnvironment> breaks;
    std::vector<BindingEnvironment> continues;
    std::vector<BindingEnvironment> discards;
};


/* ----- Symbolic program analysis ----- */

class OpaqueProgramAnalyzer
{
    public:

        OpaqueProgramAnalyzer(Program& program, OpaqueTypeLayoutCache& layouts, OpaqueProgramPlan& plan);

        // Runs the whole analysis: builds all function contracts, analyzes
        // every (canonical) function body, then derives the function ABIs.
        // The plan passed to the constructor is complete afterwards.
        void BuildAndAnalyze();

    private:

        /* ----- Contract and ABI construction ----- */

        // Builds a FunctionContract for every function in the program.
        // Rejects opaque-bearing return-by-value declarations of native
        // opaque types (R_OpaqueStructNoReturn), native opaque out/inout
        // parameters (R_OpaqueStructNoOutInout), and default arguments on
        // opaque-bearing parameters (R_OpaqueTypeDefaultArgument).
        void BuildContracts();

        // Derives a FunctionABI per contract: which parameters keep their
        // residual declaration, and which opaque lanes become scalar
        // parameters vs. grouped array parameters (using the dynamic lane
        // groups discovered during analysis).
        void BuildABIs();

        /* ----- Statement-level dataflow ----- */

        // Analyzes one function body (memoized via plan_.summaries); seeds
        // input-parameter lanes as FormalLane bindings and joins the output
        // parameters' values over all exits. Rejects recursion
        // (R_IllegalRecursiveCall).
        void AnalyzeFunction(FunctionDecl* funcDecl);

        // Folds AnalyzeStatement over a statement list, threading the
        // fall-through environment and collecting abnormal exits.
        FlowResult AnalyzeStatements(const std::vector<StmntPtr>& statements, BindingEnvironment env);

        // Abstractly executes a single statement.
        FlowResult AnalyzeStatement(const StmntPtr& statement, BindingEnvironment env);

        // Merges the flow results of two alternative branches (if/else).
        static void MergeExits(FlowResult& result, const FlowResult& lhs, const FlowResult& rhs);

        /* ----- Expression-level symbolic evaluation ----- */

        // Evaluates an expression to its symbolic OpaqueValue (if any),
        // updating 'env' for side effects. 'expectedLayout'/'expectedNode'
        // provide the declared shape when evaluating initializer contexts.
        OpaqueValue Evaluate(const ExprPtr& expr, BindingEnvironment& env, const OpaqueTypeLayoutPtr& expectedLayout, const LayoutNodePtr& expectedNode);

        // Evaluates an initializer list against the expected layout, filling
        // one binding per opaque lane from the matching source slot.
        OpaqueValue EvaluateInitializer(InitializerExpr& initializer, BindingEnvironment& env, const OpaqueTypeLayoutPtr& layout, const LayoutNodePtr& node);

        // Returns the Opaque leaf node owning the given lane index, or null.
        LayoutNodePtr FindLaneNode(const LayoutNodePtr& node, std::size_t laneIndex);

        // Resolves an object/array access chain rooted in a tracked variable
        // to the projected sub-value.
        OpaqueValue EvaluateAccess(const ExprPtr& expr, BindingEnvironment& env);

        // Projects a root value through a full access description (constant
        // steps and dynamic steps, in path order).
        OpaqueValue Project(const OpaqueValue& rootValue, const AccessDescription& access, const AST* context);

        // Selects one statically known child (field or fixed array element)
        // and slices the parent's bindings down to the lanes below that child.
        OpaqueValue ProjectRelative(const OpaqueValue& value, const PathStep& step, const AST* context);

        // Projects a fixed-size array through a runtime index. It first uses
        // ProjectRelative to enumerate the symbolic value of every possible
        // fixed element, then generalizes each corresponding element lane into
        // one dynamically indexed binding.
        OpaqueValue ProjectDynamic(const OpaqueValue& value, const ExprPtr& indexExpr, const AST* context);

        // Generalizes one relative lane across all possible fixed elements.
        // The lane must be either a contiguous axis of one resource array or a
        // sequence of lanes belonging to one formal parameter. The latter is
        // recorded in plan_.dynamicLaneGroups so the ABI can synthesize an
        // opaque-array parameter. Raises R_OpaqueTypeInvalidRuntimeIndex when
        // the alternatives cannot be represented by one runtime-indexed lane.
        OpaqueBinding GeneralizeDynamicLane(const std::vector<OpaqueValue>& elementCandidates, std::size_t relativeLaneIndex, const ExprPtr& indexExpr, const AST* context);

        // Handles an assignment whose lvalue is a tracked opaque-bearing
        // access: projects the target, evaluates the rvalue against the
        // target shape, and copies the lane bindings. Rejects dynamically
        // indexed writes (R_OpaqueTypeDynamicWrite).
        OpaqueValue EvaluateAssignment(AssignExpr& assign, BindingEnvironment& env);

        // Copies the lane bindings of 'source' into the lanes of
        // 'destination' addressed by 'targetNode'.
        static void AssignProjected(OpaqueValue& destination, const LayoutNodePtr& targetNode, const OpaqueValue& source);

        // Evaluates a call: analyzes the callee if needed, records the
        // CallSitePlan, remaps the callee's return/out values into the
        // caller's symbolic space, and writes out-parameter bindings back
        // into 'env'. Rejects calls that rely on default arguments for
        // opaque-bearing parameters (R_OpaqueTypeDefaultArgument).
        OpaqueValue EvaluateCall(CallExpr& call, BindingEnvironment& env);

        // Substitutes FormalLane/FormalArrayLane bindings of a callee-relative
        // value with the corresponding caller-side argument bindings.
        static OpaqueValue Remap(const OpaqueValue& value, const FunctionContract& contract, const std::vector<OpaqueValue>& arguments
        );

    private:

        Program&                program_;
        OpaqueTypeLayoutCache&  layouts_;
        OpaqueProgramPlan&      plan_;
        FunctionDecl*           activeFunction_ = nullptr;
};


} // /namespace OpaqueLowering

} // /namespace Xsc


#endif



// ================================================================================
