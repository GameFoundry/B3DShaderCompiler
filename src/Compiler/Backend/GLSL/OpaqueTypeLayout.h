/*
 * OpaqueTypeLayout.h
 *
 * Declaration-backed type layouts for the opaque-type lowering pass (entry
 * point: OpaqueTypeLowering.h). This header defines the vocabulary the whole
 * pass is built on:
 *
 *  - "opaque" type: a GLSL/SPIR-V resource type (texture, sampler, buffer;
 *    see Converter::IsOpaqueTypeDenoter). GLSL forbids opaque values as
 *    struct members, by-value/return values, and in most array positions,
 *    which is exactly what this pass rewrites away.
 *  - "lane": one scalar opaque leaf within a possibly-aggregate value. A
 *    struct holding two textures has two lanes; lanes are numbered in
 *    declaration order within their layout.
 *  - "residual": the non-opaque (plain-old-data) remainder of a value after
 *    all opaque lanes are stripped out; it keeps flowing through the program
 *    as ordinary data.
 *  - "native opaque": a value whose entire type is opaque (a plain resource
 *    or an array containing nothing but resources). GLSL accepts such values
 *    directly, so they need no lane splitting and pass through unchanged.
 *
 * A layout is a tree (LayoutNode) that mirrors a declared type: structs and
 * constant-extent arrays become aggregate nodes; everything else becomes a
 * POD or opaque leaf. Layouts are built once per type and memoized
 * (OpaqueTypeLayoutCache), so lane numbers and source-slot ordinals are
 * stable and shared between the analysis and the lowering phases.
 */

#ifndef XSC_OPAQUE_TYPE_LAYOUT_H
#define XSC_OPAQUE_TYPE_LAYOUT_H


#include "Visitor.h"
#include "TypeDenoter.h"
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace Xsc
{

// Internal machinery of the opaque-type lowering pass, shared between its
// translation units only. Nothing in this namespace is part of any public
// interface.
namespace OpaqueLowering
{


/* ----- Access paths ----- */

// One step of a declaration-relative access path: either a named struct field
// or a constant array index.
struct PathStep
{
    enum class Kind
    {
        Field,
        Index,
    };

    Kind        kind        = Kind::Field;
    VarDecl*    field       = nullptr;    // Field declaration (for Kind::Field).
    int         index       = 0;          // Constant array index (for Kind::Index).

    static PathStep Field(VarDecl* field)
    {
        PathStep step;
        step.kind  = Kind::Field;
        step.field = field;
        return step;
    }

    static PathStep Index(int index)
    {
        PathStep step;
        step.kind  = Kind::Index;
        step.index = index;
        return step;
    }
};

// A sequence of steps addressing a sub-object relative to a declaration root
// (e.g. ".materials[2].albedo").
using AccessPath = std::vector<PathStep>;

// Returns true if both steps address the same field or the same constant index.
bool SameStep(const PathStep& lhs, const PathStep& rhs);

// Returns true if both paths consist of pairwise identical steps.
bool SamePath(const AccessPath& lhs, const AccessPath& rhs);

// Returns true if 'path' begins with every step of 'prefix'.
bool PathStartsWith(const AccessPath& path, const AccessPath& prefix);

// Builds an identifier fragment from a path (e.g. "albedo_0"); used to derive
// readable names for synthesized per-lane parameters.
std::string PathName(const AccessPath& path);


/* ----- Layout trees ----- */

struct LayoutNode;
using LayoutNodePtr = std::shared_ptr<LayoutNode>;

// One node of a type layout tree. Aggregates (structures and constant-extent
// arrays) carry one child per member/element; leaves are either plain old
// data or a single opaque resource (one lane).
struct LayoutNode
{
    enum class Kind
    {
        Pod,        // Leaf of plain (non-opaque) data.
        Opaque,     // Leaf that is a single opaque resource; owns exactly one lane.
        Structure,  // Aggregate backed by a StructDecl.
        Array,      // Aggregate with constant extent; one child per element.
    };

    // Aggregate child together with the step that addresses it from this node.
    struct Child
    {
        PathStep       step;
        LayoutNodePtr  node;
        std::size_t    sourceOrdinal = 0;  // First source slot covered by this child.
    };

    Kind                     kind            = Kind::Pod;
    TypeDenoterPtr           sourceType;                  // Declared type this node was built from.
    StructDecl*              structDecl      = nullptr;   // Backing declaration (for Kind::Structure).
    AccessPath               absolutePath;                // Path from the layout root to this node.
    std::vector<Child>       children;                    // Aggregate children; empty for leaves.
    std::vector<std::size_t> laneIndices;                 // Indices into OpaqueTypeLayout::lanes at or below this node.
    std::vector<std::size_t> residualSourceOrdinals;      // Source slots at or below this node that carry residual data.
    bool                     hasOpaque       = false;     // True if any opaque lane exists at or below this node.
    bool                     hasResidual     = true;      // True if any residual data exists at or below this node.
    std::size_t              sourceBegin     = 0;         // First source slot covered by this node.
    std::size_t              sourceEnd       = 0;         // One past the last source slot covered by this node.
};

// Identifies one opaque scalar leaf of a layout: where it lives (path), its
// resource type, and which declaration-order source slot it occupies.
struct OpaqueLane
{
    AccessPath      path;
    TypeDenoterPtr  type;
    std::size_t     sourceOrdinal = 0;
};

// One declaration-order leaf slot of a layout (POD or opaque). Source slots
// assign flat initializer elements to their positions: the Nth unrolled
// initializer element fills the Nth source slot.
struct SourceSlot
{
    AccessPath      path;
    TypeDenoterPtr  type;
    bool            opaque = false;
};

// Complete layout of one declared type: the layout tree plus flat,
// declaration-ordered views of its opaque lanes and leaf slots.
struct OpaqueTypeLayout
{
    LayoutNodePtr             root;         // Root of the layout tree.
    std::vector<OpaqueLane>   lanes;        // All opaque leaves, in declaration order.
    std::vector<SourceSlot>   sourceSlots;  // All leaves (POD and opaque), in declaration order.

    // Returns true if the type contains at least one opaque lane.
    bool HasOpaque() const
    {
        return root && root->hasOpaque;
    }

    // Returns true if the type contains at least some non-opaque (residual) data.
    bool HasResidual() const
    {
        return root && root->hasResidual;
    }

    // Returns true if the type is exactly one opaque resource (single lane, no aggregate).
    bool IsPlainOpaque() const
    {
        return root && root->kind == LayoutNode::Kind::Opaque;
    }

    // Returns true if the type is "native opaque": a plain resource or a
    // (multi-dimensional) array containing nothing but resources. Such values
    // are legal GLSL as-is and are passed through without lane splitting.
    bool IsNativeOpaqueInput() const
    {
        return root && IsNativeOpaqueNode(root);
    }

    private:

        static bool IsNativeOpaqueNode(const LayoutNodePtr& node);
};

using OpaqueTypeLayoutPtr = std::shared_ptr<OpaqueTypeLayout>;


/* ----- Layout construction ----- */

// Builds and memoizes OpaqueTypeLayout objects. Layouts are keyed by the
// type's string form (TypeDenoter::ToString), so structurally identical
// declarations share one layout and therefore agree on lane numbering.
class OpaqueTypeLayoutCache
{
    public:

        // Returns the (cached) layout for the specified type, or null if 'type'
        // is null. 'context' is only used to attribute errors raised while building.
        OpaqueTypeLayoutPtr Get(const TypeDenoterPtr& type, const AST* context);

        // Returns the layout for the declared type of 'varDecl' (null for null input).
        OpaqueTypeLayoutPtr Get(VarDecl* varDecl, const AST* context);

        // Walks the layout tree from 'root' along 'path'; returns the addressed
        // node, or null if any step does not exist.
        LayoutNodePtr Find(const LayoutNodePtr& root, const AccessPath& path) const;

    private:

        // Recursively builds the layout node for 'type' at 'path', appending
        // lanes and source slots to 'layout' in declaration order. 'stack'
        // detects self-containing structs. Raises R_OpaqueTypeUnsizedArray for
        // opaque-bearing arrays without a fixed positive extent, and
        // R_IllegalRecursiveInheritance for recursive struct nesting.
        LayoutNodePtr Build(
            const TypeDenoterPtr& type,
            const AccessPath& path,
            OpaqueTypeLayout& layout,
            std::unordered_set<const StructDecl*>& stack,
            const AST* context
        );

        // Turns 'node' into a POD leaf occupying one new residual source slot.
        void AddPodSource(LayoutNode& node, const TypeDenoterPtr& type, const AccessPath& path, OpaqueTypeLayout& layout);

        // Returns true if 'type' (transitively, through arrays and structs)
        // contains an opaque type.
        static bool TypeContainsOpaque(const TypeDenoterPtr& type);

        // Appends one child per member variable of 'structDecl' to 'node',
        // base-struct members first (declaration order).
        void AddStructChildren(
            const LayoutNodePtr& node,
            StructDecl* structDecl,
            const AccessPath& path,
            OpaqueTypeLayout& layout,
            std::unordered_set<const StructDecl*>& stack,
            const AST* context
        );

        // Expands array axis 'dim' into per-element children; inner axes become
        // nested Array nodes, the innermost axis builds the element type itself.
        // Raises R_OpaqueTypeUnsizedArray if the axis has no positive constant extent.
        void BuildArrayDimensions(
            const LayoutNodePtr& node,
            const TypeDenoterPtr& elementType,
            const std::vector<ArrayDimensionPtr>& dims,
            std::size_t dim,
            const AccessPath& path,
            OpaqueTypeLayout& layout,
            std::unordered_set<const StructDecl*>& stack,
            const AST* context
        );

        // Folds the children's opaque/residual flags, lane indices, and residual
        // source ordinals into the aggregate 'node'.
        static void FinishAggregate(const LayoutNodePtr& node, const OpaqueTypeLayout& layout);

    private:

        std::unordered_map<std::string, OpaqueTypeLayoutPtr> cache_;
};


} // /namespace OpaqueLowering

} // /namespace Xsc


#endif



// ================================================================================
