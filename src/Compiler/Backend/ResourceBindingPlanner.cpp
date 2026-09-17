/*
 * ResourceBindingPlanner.cpp
 *
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include "ResourceBindingPlanner.h"
#include "ASTFactory.h"


namespace Xsc
{


ResourceBindingPlanner::ResourceBindingPlanner(
    ShaderTarget shaderTarget, int autoBindingStartSlot, int bindlessBindingStartSlot) :
    shaderTarget_               { shaderTarget               },
    autoBindingStartSlot_       { autoBindingStartSlot       },
    bindlessBindingStartSlot_   { bindlessBindingStartSlot   }
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
    nextBindlessBindingSlots_.clear();

    // Collect source resource bindings.
    Visit(&program);

    // Reserve explicit source bindings before assigning any free slots.
    for (const auto& binding : sourceBindings_)
    {
        if (auto reg = Register::GetForTarget(*binding.registers, shaderTarget_))
            Reserve(reg->slot, reg->space);
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

    // Assign compiler-generated bindless bindings after source resources.
    for (auto& binding : bindlessBindings)
        binding.location = TakeNextFreeSlot(binding.set, bindlessBindingStartSlot_, nextBindlessBindingSlots_);

    return (!sourceBindings_.empty() || !bindlessBindings.empty());
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
