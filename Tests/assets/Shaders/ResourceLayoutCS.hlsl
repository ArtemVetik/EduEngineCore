// Used by ShaderResourcesTests: covers every category EduBinding sorts resources into.
// Every resource is used by the entry point, otherwise reflection would not report it.

cbuffer cbPass : register(b0)
{
    float4 gPassValue;
}

cbuffer cbObject : register(b1)
{
    float4 gObjectValue;
}

Texture2D<float4> gAlbedo : register(t0);
Texture2D<float4> gNormal : register(t1);
TextureCube<float4> gEnvironment : register(t2);
Texture2D<float4> gTextureArray[4] : register(t3);
StructuredBuffer<float4> gInstances : register(t7);
ByteAddressBuffer gRawData : register(t8);
Buffer<float4> gTypedBuffer : register(t9);

Texture2D<float4> gUnboundedArray[] : register(t0, space1);

RWTexture2D<float4> gOutputTexture : register(u0);
RWStructuredBuffer<float4> gResults : register(u1);
RWByteAddressBuffer gRawOutput : register(u2);
RWBuffer<float> gTypedOutput : register(u3);

SamplerState gSampler : register(s0);

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    float2 uv = float2(id.xy) / 64.0f;

    float4 color = gAlbedo.SampleLevel(gSampler, uv, 0);
    color += gNormal.SampleLevel(gSampler, uv, 0);
    color += gEnvironment.SampleLevel(gSampler, float3(uv, 1.0f), 0);
    color += gTextureArray[id.x % 4].SampleLevel(gSampler, uv, 0);
    color += gUnboundedArray[NonUniformResourceIndex(id.x)].SampleLevel(gSampler, uv, 0);

    color += gInstances[id.x];
    color += asfloat(gRawData.Load4(id.x * 16));
    color += gTypedBuffer[id.x];
    color += gPassValue + gObjectValue;

    gOutputTexture[id.xy] = color;
    gResults[id.x] = color;
    gRawOutput.Store(id.x * 4, asuint(color.x));
    gTypedOutput[id.x] = color.y;
}
