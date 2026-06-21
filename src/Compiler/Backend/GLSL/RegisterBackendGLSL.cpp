/*
 * RegisterBackendGLSL.cpp
 *
 * Registers the built-in GLSL/ESSL/VKSL output backend.
 */

#include "BackendRegistry.h"
#include "GLSLGenerator.h"
#include <Xsc/Targets.h>
#include <memory>


namespace Xsc
{


void RegisterBackend_GLSL()
{
    /* GLSL, ESSL, and VKSL output is all produced by the GLSLGenerator. */
    const BackendDescriptor::Factory factory = [](Log* log) -> std::unique_ptr<Generator>
    {
        return std::unique_ptr<Generator>(new GLSLGenerator(log));
    };

    auto& registry = BackendRegistry::Instance();

    registry.Register(TargetLanguage::GLSL110, { OutputShaderVersion::GLSL110, factory });
    registry.Register(TargetLanguage::GLSL120, { OutputShaderVersion::GLSL120, factory });
    registry.Register(TargetLanguage::GLSL130, { OutputShaderVersion::GLSL130, factory });
    registry.Register(TargetLanguage::GLSL140, { OutputShaderVersion::GLSL140, factory });
    registry.Register(TargetLanguage::GLSL150, { OutputShaderVersion::GLSL150, factory });
    registry.Register(TargetLanguage::GLSL330, { OutputShaderVersion::GLSL330, factory });
    registry.Register(TargetLanguage::GLSL400, { OutputShaderVersion::GLSL400, factory });
    registry.Register(TargetLanguage::GLSL410, { OutputShaderVersion::GLSL410, factory });
    registry.Register(TargetLanguage::GLSL420, { OutputShaderVersion::GLSL420, factory });
    registry.Register(TargetLanguage::GLSL430, { OutputShaderVersion::GLSL430, factory });
    registry.Register(TargetLanguage::GLSL440, { OutputShaderVersion::GLSL440, factory });
    registry.Register(TargetLanguage::GLSL450, { OutputShaderVersion::GLSL450, factory });
    registry.Register(TargetLanguage::GLSL,    { OutputShaderVersion::GLSL,    factory });

    registry.Register(TargetLanguage::ESSL100, { OutputShaderVersion::ESSL100, factory });
    registry.Register(TargetLanguage::ESSL300, { OutputShaderVersion::ESSL300, factory });
    registry.Register(TargetLanguage::ESSL310, { OutputShaderVersion::ESSL310, factory });
    registry.Register(TargetLanguage::ESSL320, { OutputShaderVersion::ESSL320, factory });
    registry.Register(TargetLanguage::ESSL,    { OutputShaderVersion::ESSL,    factory });

    registry.Register(TargetLanguage::VKSL450, { OutputShaderVersion::VKSL450, factory });
    registry.Register(TargetLanguage::VKSL,    { OutputShaderVersion::VKSL,    factory });
}


} // /namespace Xsc



// ================================================================================
