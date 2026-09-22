// Used by ShaderCacheTests. Does not compile on purpose.

RWStructuredBuffer<float> gOutput : register(u0);

[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    gOutput[id.x] = UndeclaredVariable;
}
