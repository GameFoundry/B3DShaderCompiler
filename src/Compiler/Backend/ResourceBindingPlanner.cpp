/*
 * ResourceBindingPlanner.cpp
 *
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include "ResourceBindingPlanner.h"
#include "ASTFactory.h"
#include "ReportIdents.h"
#include <stdexcept>


namespace Xsc
{


ResourceBindingPlanner::ResourceBindingPlanner(
    ShaderTarget shaderTarget, int autoBindingStartSlot, int bindlessBindingStartSlot, BindlessBindingLayout bindlessBindingLayout) :
    shaderTarget_               { shaderTarget               },
    autoBindingStartSlot_       { autoBindingStartSlot       },
    bindlessBindingStartSlot_   { bindlessBindingStartSlot   },
    bindlessBindingLayout_      { bindlessBindingLayout      }
{
}

void ResourceBindingPlanner::Reserve(int slot, int space)
{
    if (slot >= 0 && space >= 0)
        usedSlots_[space].insert(slot);
}

bool ResourceBindingPlanner::Plan(
    Program& program, bool autoBinding, std::vector<Reflection::BindlessBinding>& bindlessBindings)
{
    sourceBindings_.clear();
    nextAutoBindingSlots_.clear();

    // Collect source resource bindings.
    Visit(&program);

    // Reserve explicit source bindings before assigning any free slots.
    for (const auto& binding : sourceBindings_)
    {
        if (auto reg = Register::GetForTarget(*binding.registers, shaderTarget_))
            Reserve(reg->slot, reg->space);
    }

    // Bindless bindings use fixed slots. Typed views of one descriptor type share a slot, so only explicit source bindings can collide.
    std::set<std::pair<int, int>> bindlessSlots;
    for (auto& binding : bindlessBindings)
    {
        binding.location = bindlessBindingStartSlot_ + GetBindlessSlotOffset(binding);
        bindlessSlots.insert({ binding.set, binding.location });
    }

    for (const auto& slot : bindlessSlots)
    {
        if (!usedSlots_[slot.first].insert(slot.second).second)
            throw std::runtime_error(R_BindlessBindingCollision(std::to_string(slot.second), std::to_string(slot.first)));
    }

    if (autoBinding)
    {
        // Assign missing source bindings after every explicit slot is known.
        for (const auto& binding : sourceBindings_)
        {
            auto reg = Register::GetForTarget(*binding.registers, shaderTarget_);
            if (reg != nullptr)
            {
                if (reg->registerType == RegisterType::Undefined || reg->registerType == RegisterType::BufferOffset)
                    reg->registerType = binding.registerType;

                if (reg->space < 0)
                    reg->space = 0;
                if (reg->slot < 0)
                    reg->slot = TakeNextFreeSlot(reg->space, autoBindingStartSlot_, nextAutoBindingSlots_);
            }
            else
            {
                auto newRegister = ASTFactory::MakeRegister(TakeNextFreeSlot(0, autoBindingStartSlot_, nextAutoBindingSlots_), 0, binding.registerType);

                if (!binding.registers->empty())
                    newRegister->shaderTarget = shaderTarget_;

                binding.registers->push_back(newRegister);
            }
        }
    }

    return (!sourceBindings_.empty() || !bindlessBindings.empty());
}

int ResourceBindingPlanner::GetBindlessSlotOffset(const Reflection::BindlessBinding& binding) const
{
    if (binding.heap == Reflection::BindlessHeapKind::Sampler)
        return 0;

    if (bindlessBindingLayout_ == BindlessBindingLayout::PerHeap || binding.bindingClass == Reflection::BindlessBindingClass::DescriptorBuffer)
        return 1;

    switch (binding.bindingClass)
    {
        case Reflection::BindlessBindingClass::SampledImageArray:           return 1;
        case Reflection::BindlessBindingClass::StorageImageArray:           return 2;
        case Reflection::BindlessBindingClass::UniformTexelBufferArray:     return 3;
        case Reflection::BindlessBindingClass::StorageTexelBufferArray:     return 4;
        case Reflection::BindlessBindingClass::ReadOnlyStorageBufferArray:
        case Reflection::BindlessBindingClass::ReadWriteStorageBufferArray: return 5;
        default:                                                            throw std::runtime_error(R_InvalidBindlessBindingClass);
    }
}

void ResourceBindingPlanner::AddSourceBinding(std::vector<RegisterPtr>& registers, RegisterType registerType)
{
    sourceBindings_.push_back({ &registers, registerType });
}

int ResourceBindingPlanner::TakeNextFreeSlot(int space, int startSlot, std::map<int, int>& nextSlots)
{
    auto next = nextSlots.find(space);
    if (next == nextSlots.end())
        next = nextSlots.insert({ space, startSlot }).first;

    auto& usedSlots = usedSlots_[space];
    while (usedSlots.count(next->second) > 0)
        ++next->second;

    usedSlots.insert(next->second);
    return next->second++;
}


#define IMPLEMENT_VISIT_PROC(AST_NAME) \
    void ResourceBindingPlanner::Visit##AST_NAME(AST_NAME* ast, void* args)

IMPLEMENT_VISIT_PROC(BufferDecl)
{
    AddSourceBinding(
        ast->slotRegisters,
        IsRWBufferType(ast->GetBufferType()) ? RegisterType::UnorderedAccessView : RegisterType::TextureBuffer);
    VISIT_DEFAULT(BufferDecl);
}

IMPLEMENT_VISIT_PROC(SamplerDecl)
{
    AddSourceBinding(ast->slotRegisters, RegisterType::Sampler);
    VISIT_DEFAULT(SamplerDecl);
}

IMPLEMENT_VISIT_PROC(UniformBufferDecl)
{
    if ((ast->extModifiers & ExtModifiers::PushConstant) == 0)
        AddSourceBinding(ast->slotRegisters, RegisterType::ConstantBuffer);
    VISIT_DEFAULT(UniformBufferDecl);
}

#undef IMPLEMENT_VISIT_PROC


} // /namespace Xsc



// ================================================================================
