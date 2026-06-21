
// Strict-HLSL negative fixture: 'precise' type modifier.
// Without -Xstrict-hlsl, parsed and emitted as-is (no portable cross-target
// equivalent, so the modifier silently disappears in stricter backends).
// With -Xstrict-hlsl, the analyzer must reject 'precise' explicitly.

float4 VS(float3 pos : POSITION) : SV_Position
{
    precise float3 scaled = pos * 0.5;
    return float4(scaled, 1.0);
}
