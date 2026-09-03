/*
 * RegisterBackendHLSL.cpp
 *
 * Registers the HLSL-to-HLSL "round-trip" output backend. 
 */

#include "BackendRegistry.h"
#include "HLSLGenerator.h"
#include <Xsc/Targets.h>
#include <memory>


namespace Xsc
{


void RegisterBackend_HLSL()
{
    /* HLSL-to-HLSL "round-trip" output is produced by the HLSLGenerator. */
    const BackendDescriptor::Factory factory = [](Log* log) -> std::unique_ptr<Generator>
    {
        return std::unique_ptr<Generator>(new HLSLGenerator(log));
    };

    auto& registry = BackendRegistry::Instance();

    BackendDescriptor hlsl5 { OutputShaderVersion::HLSL5, factory };
    BackendDescriptor hlsl6 { OutputShaderVersion::HLSL6, factory };
    BackendDescriptor hlsl  { OutputShaderVersion::HLSL,  factory };

    hlsl6.features |= BackendDescriptor::BindlessResources;
    hlsl.features  |= BackendDescriptor::BindlessResources;

    registry.Register(TargetLanguage::HLSL5, hlsl5);
    registry.Register(TargetLanguage::HLSL6, hlsl6);
    registry.Register(TargetLanguage::HLSL,  hlsl);
}


} // /namespace Xsc



// ================================================================================
