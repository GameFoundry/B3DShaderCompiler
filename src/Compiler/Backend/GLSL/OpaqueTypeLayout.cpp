/*
 * OpaqueTypeLayout.cpp
 */

#include "OpaqueTypeLayout.h"
#include "AST.h"
#include "Converter.h"
#include "Exception.h"
#include "ReportIdents.h"

#include <algorithm>


namespace Xsc
{

namespace OpaqueLowering
{


/* ----- Access path helpers ----- */

bool SameStep(const PathStep& lhs, const PathStep& rhs)
{
    if (lhs.kind != rhs.kind)
        return false;
    return (lhs.kind == PathStep::Kind::Field ? lhs.field == rhs.field : lhs.index == rhs.index);
}

bool SamePath(const AccessPath& lhs, const AccessPath& rhs)
{
    if (lhs.size() != rhs.size())
        return false;
    for (std::size_t i = 0; i < lhs.size(); ++i)
    {
        if (!SameStep(lhs[i], rhs[i]))
            return false;
    }
    return true;
}

bool PathStartsWith(const AccessPath& path, const AccessPath& prefix)
{
    if (path.size() < prefix.size())
        return false;
    for (std::size_t i = 0; i < prefix.size(); ++i)
    {
        if (!SameStep(path[i], prefix[i]))
            return false;
    }
    return true;
}

std::string PathName(const AccessPath& path)
{
    std::string result;
    for (const auto& step : path)
    {
        if (step.kind == PathStep::Kind::Field)
        {
            if (!result.empty())
                result += "_";
            result += (step.field != nullptr ? step.field->ident.Original() : std::string("field"));
        }
        else
        {
            if (!result.empty())
                result += "_";
            result += std::to_string(step.index);
        }
    }
    return result;
}


/* ----- OpaqueTypeLayout ----- */

bool OpaqueTypeLayout::IsNativeOpaqueNode(const LayoutNodePtr& node)
{
    if (!node)
        return false;
    if (node->kind == LayoutNode::Kind::Opaque)
        return true;
    if (node->kind != LayoutNode::Kind::Array || node->children.empty())
        return false;
    for (const auto& child : node->children)
    {
        if (!IsNativeOpaqueNode(child.node))
            return false;
    }
    return true;
}


/* ----- OpaqueTypeLayoutCache ----- */

OpaqueTypeLayoutPtr OpaqueTypeLayoutCache::Get(const TypeDenoterPtr& type, const AST* context)
{
    if (!type)
        return nullptr;

    const std::string key = type->ToString();
    auto it = cache_.find(key);
    if (it != cache_.end())
        return it->second;

    auto layout = std::make_shared<OpaqueTypeLayout>();
    std::unordered_set<const StructDecl*> stack;
    AccessPath path;
    layout->root = Build(type, path, *layout, stack, context);
    cache_[key] = layout;
    return layout;
}

OpaqueTypeLayoutPtr OpaqueTypeLayoutCache::Get(VarDecl* varDecl, const AST* context)
{
    return (varDecl != nullptr ? Get(varDecl->GetTypeDenoter(), context) : nullptr);
}

LayoutNodePtr OpaqueTypeLayoutCache::Find(const LayoutNodePtr& root, const AccessPath& path) const
{
    auto node = root;
    for (const auto& step : path)
    {
        if (!node)
            return nullptr;
        auto it = std::find_if(
            node->children.begin(), node->children.end(),
            [&](const LayoutNode::Child& child) { return SameStep(child.step, step); }
        );
        if (it == node->children.end())
            return nullptr;
        node = it->node;
    }
    return node;
}

LayoutNodePtr OpaqueTypeLayoutCache::Build(
    const TypeDenoterPtr& type,
    const AccessPath& path,
    OpaqueTypeLayout& layout,
    std::unordered_set<const StructDecl*>& stack,
    const AST* context)
{
    auto node = std::make_shared<LayoutNode>();
    node->sourceType  = type;
    node->absolutePath = path;
    node->sourceBegin = layout.sourceSlots.size();

    const TypeDenoter& aliased = type->GetAliased();
    if (Converter::IsOpaqueTypeDenoter(type))
    {
        node->kind        = LayoutNode::Kind::Opaque;
        node->hasOpaque   = true;
        node->hasResidual = false;

        SourceSlot source;
        source.path   = path;
        source.type   = type;
        source.opaque = true;
        const std::size_t sourceOrdinal = layout.sourceSlots.size();
        layout.sourceSlots.push_back(source);

        OpaqueLane lane;
        lane.path          = path;
        lane.type          = type;
        lane.sourceOrdinal = sourceOrdinal;
        node->laneIndices.push_back(layout.lanes.size());
        layout.lanes.push_back(lane);
    }
    else if (auto arrayType = aliased.As<ArrayTypeDenoter>())
    {
        node->kind        = LayoutNode::Kind::Array;
        node->hasOpaque   = false;
        node->hasResidual = false;

        bool hasUnsizedAxis = (!arrayType->subTypeDenoter || arrayType->arrayDims.empty());
        for (const auto& dim : arrayType->arrayDims)
            hasUnsizedAxis = hasUnsizedAxis || !dim || dim->size <= 0;

        if (hasUnsizedAxis)
        {
            if (arrayType->subTypeDenoter && TypeContainsOpaque(arrayType->subTypeDenoter))
                RuntimeErr(R_OpaqueTypeUnsizedArray(type->ToString()), context);
            /* Dynamic POD arrays are unrelated to this feature and retain the
               frontend's existing treatment. */
            AddPodSource(*node, type, path, layout);
        }
        else
        {
            BuildArrayDimensions(node, arrayType->subTypeDenoter, arrayType->arrayDims, 0, path, layout, stack, context);
            FinishAggregate(node, layout);
        }
    }
    else if (auto structType = aliased.As<StructTypeDenoter>())
    {
        node->kind       = LayoutNode::Kind::Structure;
        node->structDecl = structType->structDeclRef;
        if (!node->structDecl)
        {
            node->kind = LayoutNode::Kind::Pod;
            AddPodSource(*node, type, path, layout);
        }
        else
        {
            if (!stack.insert(node->structDecl).second)
                RuntimeErr(R_IllegalRecursiveInheritance, context);

            AddStructChildren(node, node->structDecl, path, layout, stack, context);
            stack.erase(node->structDecl);
            FinishAggregate(node, layout);
        }
    }
    else
        AddPodSource(*node, type, path, layout);

    node->sourceEnd = layout.sourceSlots.size();
    return node;
}

void OpaqueTypeLayoutCache::AddPodSource(LayoutNode& node, const TypeDenoterPtr& type, const AccessPath& path, OpaqueTypeLayout& layout)
{
    node.kind        = LayoutNode::Kind::Pod;
    node.hasOpaque   = false;
    node.hasResidual = true;
    SourceSlot source;
    source.path   = path;
    source.type   = type;
    source.opaque = false;
    node.residualSourceOrdinals.push_back(layout.sourceSlots.size());
    layout.sourceSlots.push_back(source);
}

bool OpaqueTypeLayoutCache::TypeContainsOpaque(const TypeDenoterPtr& type)
{
    if (!type)
        return false;
    if (Converter::IsOpaqueTypeDenoter(type))
        return true;
    const auto& aliased = type->GetAliased();
    if (auto arrayType = aliased.As<ArrayTypeDenoter>())
        return TypeContainsOpaque(arrayType->subTypeDenoter);
    if (auto structType = aliased.As<StructTypeDenoter>())
        return structType->structDeclRef && structType->structDeclRef->HasOpaqueMember();
    return false;
}

void OpaqueTypeLayoutCache::AddStructChildren(
    const LayoutNodePtr& node,
    StructDecl* structDecl,
    const AccessPath& path,
    OpaqueTypeLayout& layout,
    std::unordered_set<const StructDecl*>& stack,
    const AST* context)
{
    if (structDecl->baseStructRef)
        AddStructChildren(node, structDecl->baseStructRef, path, layout, stack, context);

    for (const auto& memberStmnt : structDecl->varMembers)
    {
        for (const auto& member : memberStmnt->varDecls)
        {
            AccessPath childPath = path;
            childPath.push_back(PathStep::Field(member.get()));
            LayoutNode::Child child;
            child.step          = PathStep::Field(member.get());
            child.sourceOrdinal = layout.sourceSlots.size();
            child.node          = Build(member->GetTypeDenoter(), childPath, layout, stack, context);
            node->children.push_back(child);
        }
    }
}

void OpaqueTypeLayoutCache::BuildArrayDimensions(
    const LayoutNodePtr& node,
    const TypeDenoterPtr& elementType,
    const std::vector<ArrayDimensionPtr>& dims,
    std::size_t dim,
    const AccessPath& path,
    OpaqueTypeLayout& layout,
    std::unordered_set<const StructDecl*>& stack,
    const AST* context)
{
    if (dim >= dims.size())
        return;
    const int extent = (dims[dim] != nullptr ? dims[dim]->size : 0);
    if (extent <= 0)
        RuntimeErr(R_OpaqueTypeUnsizedArray(elementType->ToString()), context);

    for (int i = 0; i < extent; ++i)
    {
        AccessPath childPath = path;
        childPath.push_back(PathStep::Index(i));
        LayoutNode::Child child;
        child.step          = PathStep::Index(i);
        child.sourceOrdinal = layout.sourceSlots.size();
        if (dim + 1 < dims.size())
        {
            child.node = std::make_shared<LayoutNode>();
            child.node->kind          = LayoutNode::Kind::Array;
            child.node->sourceType    = elementType;
            child.node->absolutePath  = childPath;
            child.node->sourceBegin   = layout.sourceSlots.size();
            child.node->hasResidual   = false;
            BuildArrayDimensions(child.node, elementType, dims, dim + 1, childPath, layout, stack, context);
            FinishAggregate(child.node, layout);
            child.node->sourceEnd = layout.sourceSlots.size();
        }
        else
            child.node = Build(elementType, childPath, layout, stack, context);
        node->children.push_back(child);
    }
}

void OpaqueTypeLayoutCache::FinishAggregate(const LayoutNodePtr& node, const OpaqueTypeLayout& layout)
{
    node->hasOpaque   = false;
    node->hasResidual = false;
    for (const auto& child : node->children)
    {
        node->hasOpaque   = node->hasOpaque   || child.node->hasOpaque;
        node->hasResidual = node->hasResidual || child.node->hasResidual;
        node->laneIndices.insert(node->laneIndices.end(), child.node->laneIndices.begin(), child.node->laneIndices.end());
        node->residualSourceOrdinals.insert(
            node->residualSourceOrdinals.end(),
            child.node->residualSourceOrdinals.begin(), child.node->residualSourceOrdinals.end()
        );
    }
}


} // /namespace OpaqueLowering

} // /namespace Xsc



// ================================================================================
