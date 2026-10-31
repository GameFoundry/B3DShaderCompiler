// Descriptor heap accesses returned from functions, bare and inside structs, with index expressions built from the callee's parameters and locals.
cbuffer Indices : register(b0, space1) { uint TextureIndex; uint SamplerIndex; uint Offset; };
RWStructuredBuffer<float4> Output : register(u0, space1);

struct GlobalSampler { SamplerState Sampler; };
struct Material { Texture2D<float4> Albedo; SamplerState Sampler; float4 Tint; };

SamplerState GetSampler(uint index) { return SamplerDescriptorHeap[index]; }
SamplerState GetOffsetSampler(uint index) { uint local = index * 2 + Offset; return SamplerDescriptorHeap[local + 1]; }
Texture2D<float4> GetTexture(uint index) { return ResourceDescriptorHeap[NonUniformResourceIndex(index + 1)]; }

GlobalSampler GetGlobalSampler(uint index)
{
    GlobalSampler result;
    result.Sampler = SamplerDescriptorHeap[1 + index];
    return result;
}

// Nested: the wrapper returns what its callee exported, so it must export it again.
SamplerState GetSamplerNested(uint index) { return GetGlobalSampler(index + 2).Sampler; }
Texture2D<float4> GetTextureNested(uint index) { return GetTexture(index * 3); }

// Mixed residual and opaque members, returned from more than one exit.
Material GetMaterial(uint index, bool alternate)
{
    Material material;
    material.Albedo = ResourceDescriptorHeap[index];
    material.Sampler = SamplerDescriptorHeap[index + 4];
    material.Tint = float4(1, 1, 1, 1);
    if (alternate)
    {
        material.Tint = float4(0.5, 0.5, 0.5, 1);
        return material;
    }
    return material;
}

// An output parameter exports its index too, from a function that still returns a value.
float FillSampler(uint index, out GlobalSampler result)
{
    result.Sampler = SamplerDescriptorHeap[index + 5];
    return 2.0f;
}

void FillTexture(uint index, out Material material)
{
    material.Albedo = ResourceDescriptorHeap[index + 6];
    material.Sampler = SamplerDescriptorHeap[index + 7];
    material.Tint = float4(1, 0, 0, 1);
}

[numthreads(1, 1, 1)]
void CS()
{
    float2 uv = float2(0.5f, 0.5f);
    Texture2D<float4> heapTexture = ResourceDescriptorHeap[TextureIndex];
    float4 result = heapTexture.SampleLevel(GetSampler(SamplerIndex), uv, 0);
    result += heapTexture.SampleLevel(GetOffsetSampler(SamplerIndex), uv, 0);
    result += GetTexture(TextureIndex).SampleLevel(GetGlobalSampler(SamplerIndex).Sampler, uv, 0);
    result += GetTextureNested(TextureIndex).SampleLevel(GetSamplerNested(SamplerIndex), uv, 0);

    Material material = GetMaterial(TextureIndex, SamplerIndex > 3);
    result += material.Albedo.SampleLevel(material.Sampler, uv, 0) * material.Tint;

    GlobalSampler filled;
    float scale = FillSampler(SamplerIndex, filled);
    result += heapTexture.SampleLevel(filled.Sampler, uv, 0) * scale;

    Material filledMaterial;
    FillTexture(TextureIndex, filledMaterial);
    result += filledMaterial.Albedo.SampleLevel(filledMaterial.Sampler, uv, 0) * filledMaterial.Tint;

    Output[0] = result;
}
