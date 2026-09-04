/*
 * ResourceBindingPlanner.h
 *
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#ifndef XSC_RESOURCE_BINDING_PLANNER_H
#define XSC_RESOURCE_BINDING_PLANNER_H


#include "AST.h"
#include "Visitor.h"
#include <map>
#include <set>


namespace Xsc
{


/*
Coordinates source-resource and compiler-generated bindless bindings. Explicit source
bindings are collected before any missing binding is assigned, so allocation is
independent of declaration order. All resource kinds share one slot namespace
per descriptor set/register space.
*/
class ResourceBindingPlanner final : private Visitor
{

    public:

        ResourceBindingPlanner(ShaderTarget shaderTarget, int autoBindingStartSlot, int bindlessBindingStartSlot);

        // Reserves a binding that is emitted outside the source resource declarations.
        void Reserve(int slot, int space);

        // Plans source and bindless bindings. Bindless locations are always assigned here.
        // Returns true if either kind exists.
        bool Plan(Program& program, bool autoBinding, std::vector<Reflection::BindlessBinding>& bindlessBindings);

    private:

        struct SourceBinding
        {
            std::vector<RegisterPtr>* registers;
            RegisterType              registerType;
        };

        DECL_VISIT_PROC( BufferDecl        );
        DECL_VISIT_PROC( SamplerDecl       );
        DECL_VISIT_PROC( UniformBufferDecl );

        void AddSourceBinding(std::vector<RegisterPtr>& registers, RegisterType registerType);

        int TakeNextFreeSlot(int space, int startSlot, std::map<int, int>& nextSlots);

        ShaderTarget                    shaderTarget_;
        int                             autoBindingStartSlot_;
        int                             bindlessBindingStartSlot_;
        std::vector<SourceBinding>      sourceBindings_;
        std::map<int, std::set<int>>    usedSlots_;
        std::map<int, int>              nextAutoBindingSlots_;
        std::map<int, int>              nextBindlessBindingSlots_;

};


} // /namespace Xsc


#endif



// ================================================================================
