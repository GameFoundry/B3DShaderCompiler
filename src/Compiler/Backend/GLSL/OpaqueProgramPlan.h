/*
 * OpaqueProgramPlan.h
 *
 * Shared data structures of the opaque-type lowering pass: the immutable
 * per-function contracts, the symbolic values computed by the analysis, and
 * the OpaqueProgramPlan that carries all analysis results into the lowering.
 *
 * The pass runs in two phases over an untouched program (see
 * OpaqueTypeLowering.cpp for the driver):
 *
 *  1. OpaqueProgramAnalyzer builds a FunctionContract (declared shape) and a
 *     FunctionABI (planned rewrite) for every function, then symbolically
 *     executes each function body to determine, for every opaque lane of
 *     every value, an OpaqueBinding: which concrete resource — or which lane
 *     of a formal parameter — that lane is bound to. All results are written
 *     into an OpaqueProgramPlan.
 *  2. OpaqueASTLowerer consumes the finished plan (by const reference) and
 *     rewrites signatures, call sites, initializers, returns, and type
 *     declarations accordingly.
 *
 * Everything in this header is plain data plus a few pure lookup helpers;
 * neither phase mutates the other's state through it.
 */

#ifndef XSC_OPAQUE_PROGRAM_PLAN_H
#define XSC_OPAQUE_PROGRAM_PLAN_H


#include "OpaqueTypeLayout.h"
#include "AST.h"
#include "Visitor.h"
#include <cstddef>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace Xsc
{

namespace OpaqueLowering
{


/* ----- Symbolic values ----- */

// One array-subscript use: either a compile-time constant or a dynamic
// (runtime) index expression.
struct IndexUse
{
    bool        dynamic = false;
    int         constant = 0;
    ExprPtr     expression;     // Runtime index expression (for dynamic == true).
};

// What a single opaque lane resolves to symbolically. This is the value
// domain of the dataflow analysis: reads are legal only for lanes that
// resolve to a concrete resource or to a lane of the enclosing function's
// own formal parameters.
struct OpaqueBinding
{
    enum class Kind
    {
        Uninitialized,       // No binding recorded yet; reading it is an error.
        Resource,            // A global resource (BufferDecl/SamplerDecl), possibly indexed.
        DescriptorHeap,      // One typed SM 6.6 descriptor-heap access.
        ResourceArraySlice,  // A contiguous slice of a global resource array.
        FormalLane,          // Lane 'laneIndex' of formal parameter 'formal'.
        FormalArrayLane,     // Like FormalLane, but dynamically indexed into a lane-group array.
        FixedElementTable,   // Per-element table of bindings ('elements'); cannot be collapsed.
        Conflict,            // Control-flow paths disagreed; reading it is an error.
    };

    Kind                            kind        = Kind::Uninitialized;
    Decl*                           resource    = nullptr;
    VarDecl*                        formal      = nullptr;
    std::size_t                     laneIndex   = 0;
    std::vector<IndexUse>           indices;
    std::vector<OpaqueBinding>      elements;
    DescriptorHeapKind              descriptorHeap = DescriptorHeapKind::Undefined;
    TypeDenoterPtr                  descriptorType;

    static OpaqueBinding Resource(Decl* decl, const std::vector<IndexUse>& indices = std::vector<IndexUse>())
    {
        OpaqueBinding binding;
        binding.kind     = Kind::Resource;
        binding.resource = decl;
        binding.indices  = indices;
        return binding;
    }

    static OpaqueBinding Formal(VarDecl* param, std::size_t laneIndex)
    {
        OpaqueBinding binding;
        binding.kind      = Kind::FormalLane;
        binding.formal    = param;
        binding.laneIndex = laneIndex;
        return binding;
    }

    static OpaqueBinding Descriptor(DescriptorHeapKind heap, const TypeDenoterPtr& type, const ExprPtr& index)
    {
        OpaqueBinding binding;
        binding.kind           = Kind::DescriptorHeap;
        binding.descriptorHeap = heap;
        binding.descriptorType = (type ? type->Copy() : nullptr);
        IndexUse indexUse;
        indexUse.dynamic    = true;
        indexUse.expression = index;
        binding.indices.push_back(indexUse);
        return binding;
    }

    static OpaqueBinding Conflict()
    {
        OpaqueBinding binding;
        binding.kind = Kind::Conflict;
        return binding;
    }
};

// Structural equality of two bindings (including index lists and element
// tables). Dynamic indices compare by expression identity.
bool SameBinding(const OpaqueBinding& lhs, const OpaqueBinding& rhs);

// Lattice join used at control-flow merge points: equal bindings pass
// through unchanged, anything else collapses to Conflict.
OpaqueBinding JoinBinding(const OpaqueBinding& lhs, const OpaqueBinding& rhs);

// The symbolic opaque content of one value at one program point: the layout
// (sub-)node describing its shape, and one binding per opaque lane below
// that node (in laneIndices order).
struct OpaqueValue
{
    OpaqueTypeLayoutPtr         layout;
    LayoutNodePtr               node;
    std::vector<OpaqueBinding>  bindings;

    bool Valid() const
    {
        return node && node->hasOpaque;
    }
};

// Dataflow state: the symbolic value of every tracked local/parameter.
using BindingEnvironment = std::unordered_map<VarDecl*, OpaqueValue>;


/* ----- Contracts and ABIs ----- */

// Declared shape of one function parameter, fixed before any rewriting.
struct ParameterContract
{
    VarDeclStmntPtr        declaration;             // The parameter's declaration statement.
    VarDecl*               variable = nullptr;      // The parameter variable itself.
    OpaqueTypeLayoutPtr    layout;                  // Layout of the declared type.
    bool                   input = false;           // 'in' (or default) parameter.
    bool                   output = false;          // 'out'/'inout' parameter.
    bool                   nativeOpaque = false;    // Entire type is opaque; passed through unchanged.
};

// Declared shape of one function, fixed before any rewriting.
struct FunctionContract
{
    FunctionDecl*                   declaration = nullptr;  // The declaration this contract was built from.
    FunctionDecl*                   canonical = nullptr;    // Implementation behind a forward declaration.
    TypeSpecifierPtr                originalReturnType;     // Return type before any lowering.
    OpaqueTypeLayoutPtr             returnLayout;           // Layout of the declared return type.
    std::vector<ParameterContract>  parameters;
};

// Planned rewrite of one parameter: whether its residual declaration
// survives, and which opaque lanes become scalar parameters vs. grouped
// array parameters (one group per dynamically indexed lane set).
struct ParameterABI
{
    bool                                    keepResidual = true;
    bool                                    nativeOpaque = false;
    std::vector<std::size_t>                scalarLanes;
    std::vector<std::vector<std::size_t>>   arrayLaneGroups;
};

// Planned rewrite of one function signature.
struct FunctionABI
{
    FunctionDecl*                 declaration = nullptr;
    std::vector<ParameterABI>     parameters;
    bool                          lowerReturn = false;      // Return type contains opaque lanes; becomes void.
    bool                          returnResidual = false;   // Lowered return still has residual data -> out-parameter.
};


/* ----- Analysis results ----- */

// Memoized dataflow result of one analyzed function: the joined symbolic
// return value and the joined symbolic values of its output parameters.
struct FunctionSummary
{
    bool                                        analyzed = false;
    bool                                        analyzing = false;  // Recursion guard.
    bool                                        hasReturn = false;
    OpaqueValue                                 returnValue;
    std::unordered_map<VarDecl*, OpaqueValue>   outValues;
};

// Symbolic bindings at one call site: the callee contract, the caller-side
// value of each argument, and the remapped result value.
struct CallSitePlan
{
    FunctionContract*          contract = nullptr;
    std::vector<OpaqueValue>   arguments;
    OpaqueValue                result;
};

// Everything the analysis learned about the program, produced once by
// OpaqueProgramAnalyzer and consumed read-only by OpaqueASTLowerer.
// 'contracts' and 'abis' run parallel; 'contractIndex' maps a FunctionDecl
// to its index in both.
struct OpaqueProgramPlan
{
    std::vector<FunctionContract>                                       contracts;
    std::vector<FunctionABI>                                            abis;
    std::unordered_map<FunctionDecl*, std::size_t>                      contractIndex;
    std::unordered_map<FunctionDecl*, FunctionSummary>                  summaries;
    std::unordered_map<const Expr*, OpaqueValue>                        expressionValues;       // Symbolic value per opaque-bearing expression.
    std::unordered_map<const CallExpr*, CallSitePlan>                   callSites;
    std::unordered_map<const AssignExpr*, bool>                         opaqueOnlyAssignments;  // True if the assignment moves no residual data.
    std::unordered_map<const VarDecl*, OpaqueTypeLayoutPtr>             variableLayouts;
    std::unordered_map<VarDecl*, std::vector<std::vector<std::size_t>>> dynamicLaneGroups;      // Lane groups that were dynamically indexed, per formal.
    std::unordered_set<VarDecl*>                                        nativeOpaqueParameters;
};


/* ----- Shared program helpers ----- */

// Resolves a forward declaration to its implementation; returns the input
// declaration unchanged otherwise (including null).
FunctionDecl* CanonicalFunction(FunctionDecl* funcDecl);

// Invokes 'callback' for every function declaration in the program: global
// functions and member functions of global structs.
void ForEachFunction(Program& program, const std::function<void(FunctionDecl*)>& callback);

// Returns the contract registered for 'function' (directly, or via its
// canonical declaration), or null if the function has no contract.
const FunctionContract* FindContract(const OpaqueProgramPlan& plan, FunctionDecl* function);
FunctionContract*       FindContract(OpaqueProgramPlan& plan, FunctionDecl* function);

// Returns the ABI registered for 'function' (same lookup as FindContract),
// or null. Only valid after OpaqueProgramAnalyzer::BuildAndAnalyze completed.
const FunctionABI* FindABI(const OpaqueProgramPlan& plan, FunctionDecl* function);


} // /namespace OpaqueLowering

} // /namespace Xsc


#endif



// ================================================================================
