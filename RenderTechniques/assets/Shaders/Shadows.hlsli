#define MAX_CASCADES 4

float3 ApplyShadowBias(float3 positionWS, float3 normalWS, float3 lightDirection, float2 shadowBias)
{
    float invNdotL = 1.0 - saturate(dot(lightDirection, normalWS));
    float scale = invNdotL * shadowBias.y;

    // normal bias is negative since we want to apply an inset normal offset
    positionWS = lightDirection * shadowBias.xxx + positionWS;
    positionWS = normalWS * scale.xxx + positionWS;
    return positionWS;
}

//---------------------------------------------------------------------------------------
// PCF for shadow mapping.
//---------------------------------------------------------------------------------------

float CalcShadowFactor(Texture2D shadowMap, SamplerComparisonState shadowSampler, float4 shadowPosH)
{
    float depth = shadowPosH.z;

    uint width, height, numMips;
    shadowMap.GetDimensions(0, width, height, numMips);
	
    float dx = 1.0f / (float) width;
    dx = 0.5 * dx;
    
    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
    };
	
    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        percentLit += shadowMap.SampleCmpLevelZero(shadowSampler, shadowPosH.xy + offsets[i], depth).r;
    }
	
    return percentLit / 9.0f;
}

uint ComputeCascadeIndex(float3 positionWS, float4 cascadeShadowSphere[MAX_CASCADES], float4 cascadeShadowRad2)
{
    float3 fromCenter0 = positionWS - cascadeShadowSphere[0].xyz;
    float3 fromCenter1 = positionWS - cascadeShadowSphere[1].xyz;
    float3 fromCenter2 = positionWS - cascadeShadowSphere[2].xyz;
    float3 fromCenter3 = positionWS - cascadeShadowSphere[3].xyz;
    float4 distances2 = float4(dot(fromCenter0, fromCenter0), dot(fromCenter1, fromCenter1), dot(fromCenter2, fromCenter2), dot(fromCenter3, fromCenter3));

    float4 weights = float4(distances2 < cascadeShadowRad2);
    weights.yzw = saturate(weights.yzw - weights.xyz);

    return 4 - dot(weights, float4(4, 3, 2, 1));
}