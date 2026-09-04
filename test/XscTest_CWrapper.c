/*
 * XscTest1.c
 * 
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include <XscC/XscC.h>
#include <stdio.h>
#include <string.h>


#define PRINT_FUNC                              \
    puts("");                                   \
    puts("");                                   \
    printf("~~~~~ %s ~~~~~\n", __FUNCTION__);   \
    puts("")

void TestGLSLExtensions()
{
    PRINT_FUNC;

    char extension[256];
    int version;

    // Get first extension
    void* iterator = XscGetGLSLExtensionEnumeration(NULL, extension, 256, &version);

    while (iterator != NULL)
    {
        // Print extension name and version
        printf("%s ( %d )\n", extension, version);
    
        // Get next extension
        iterator = XscGetGLSLExtensionEnumeration(iterator, extension, 256, &version);
    }
}

void TestShaderTarget()
{
    PRINT_FUNC;

    char target[64];

    XscShaderTargetToString(XscETargetVertexShader, target, 64);
    puts(target);

    XscShaderTargetToString(XscETargetTessellationControlShader, target, 64);
    puts(target);
}

void TestCompile()
{
    PRINT_FUNC;

    // Initialize structures
    struct XscShaderInput in;
    struct XscShaderOutput out;
    XscInitialize(&in, &out);

    const char* outputCode = NULL;

    // Specify shader code
    in.filename     = "test.hlsl";
    in.entryPoint   = "VS";
    in.shaderTarget = XscETargetVertexShader;
    in.warnings     = XscWarnBasic;
    in.sourceCode   =
    (
        "cbuffer Matrices {\n"
        "    float4x4 wvpMatrix;\n"
        "};\n"
        "float4 VS(float3 pos : POSITION) : SV_Position {\n"
        "    return mul(wvpMatrix, float4(pos, 1));\n"
        "}\n"
    );
    
    out.filename    = "test.VS.vert";
    out.sourceCode  = &outputCode;

    // Compile shader
    puts(in.sourceCode);

    struct XscReflectionData reflect;

    if (XscCompileShader(&in, &out, XSC_DEFAULT_LOG, &reflect))
    {
        puts("*** COMPILATION SUCCESSFUL ***\n");
        if (outputCode != NULL)
            puts(outputCode);
    }
    else
        puts("*** COMPILATION FAILED ***");
}

int TestPushConstants()
{
    PRINT_FUNC;

    struct XscShaderInput in;
    struct XscShaderOutput out;
    XscInitialize(&in, &out);

    const char* outputCode = NULL;
    in.filename = "push-constant-test.hlsl";
    in.entryPoint = "main";
    in.shaderTarget = XscETargetVertexShader;
    in.extensions = XscExtPushConstants;
    in.sourceCode =
    (
        "[pushConstant]\n"
        "cbuffer DrawConstants { float scalarValue; float2 vectorValue; };\n"
        "float4 main(uint id : SV_VertexID) : SV_Position {\n"
        "    return float4(vectorValue, scalarValue, float(id));\n"
        "}\n"
    );

    out.targetLanguage = XscTargetGLSL450;
    out.sourceCode = &outputCode;
    out.options.pushConstantHLSLRegister = 3;
    out.options.pushConstantHLSLRegisterSpace = 7;

    struct XscReflectionData reflect;
    if (!XscCompileShader(&in, &out, XSC_DEFAULT_LOG, &reflect))
        return 1;

    if (reflect.pushConstantBuffersCount != 1)
        return 2;

    const struct XscPushConstantBuffer* buffer = &reflect.pushConstantBuffers[0];
    if (strcmp(buffer->ident, "DrawConstants") != 0 || buffer->size != 16 || buffer->membersCount != 2)
        return 3;

    if (buffer->members[0].offset != 0 || buffer->members[0].size != 4 ||
        buffer->members[1].offset != 8 || buffer->members[1].size != 8)
    {
        return 4;
    }

    return 0;
}

int main()
{
    puts("XscTest1");

    TestGLSLExtensions();
    TestShaderTarget();
    TestCompile();

    return TestPushConstants();
}



// ================================================================================
