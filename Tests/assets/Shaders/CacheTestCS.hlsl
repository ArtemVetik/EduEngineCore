// Used by ShaderCacheTests. The shader is only compiled, never dispatched.

#ifndef GROUP_SIZE
#define GROUP_SIZE 64
#endif

#ifndef SCALE
#define SCALE 1
#endif

cbuffer cbParams : register(b0)
{
    float gMultiplier;
}

RWStructuredBuffer<float> gOutput : register(u0);

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    gOutput[id.x] = id.x * gMultiplier * SCALE;
}

[numthreads(GROUP_SIZE, 1, 1)]
void CSClear(uint3 id : SV_DispatchThreadID)
{
    gOutput[id.x] = 0.0f;
}
