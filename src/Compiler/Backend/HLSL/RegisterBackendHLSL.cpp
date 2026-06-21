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

    registry.Register(TargetLanguage::HLSL5, { OutputShaderVersion::HLSL5, factory });
    registry.Register(TargetLanguage::HLSL,  { OutputShaderVersion::HLSL,  factory });
}


} // /namespace Xsc



// ================================================================================
