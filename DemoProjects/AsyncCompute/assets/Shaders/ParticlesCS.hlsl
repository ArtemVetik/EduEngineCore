#include "ParticlesData.hlsl"
#include "SimpleRandom.hlsl"
#include "SimplexNoise.hlsl"

#define SEED_POS        0x91E10DA5
#define SEED_VELOCITY   0xC3E4D1F5
#define SEED_COLOR      0xA2C79A3B
#define SEED_LIFETIME   0x7F4A7C15
#define SEED_SIZE       0xD1B54A35
#define SEED_MISC       0xBADC0FFE

cbuffer cbPass : register(b0)
{
    uint gMaxParticlesNum;
    float gDeltaTime;
    float gTotalTime;
    uint gEmitterSeed;
}

RWStructuredBuffer<Particle> gParticlesIn : register(u0);
RWStructuredBuffer<Particle> gParticlesOut : register(u1);

uint Seed(uint base, uint salt)
{
    return hash(base ^ salt);
}

Particle SpawnParticle(uint id)
{
    uint baseSeed = hash(id ^ gEmitterSeed);
    
    Particle p;

    uint base = id;

    p.Position = randomInRect(Seed(base, SEED_POS), 10);
    p.Velocity = randomUnitVector(Seed(base, SEED_VELOCITY));
    p.Lifetime = 120;
    p.Age = 0;
    p.Color = float3(
        random(Seed(baseSeed, SEED_COLOR + 0), 0.5f, 1.0f),
        random(Seed(baseSeed, SEED_COLOR + 1), 0.5f, 1.0f),
        random(Seed(baseSeed, SEED_COLOR + 2), 0.5f, 1.0f)
    );
    
    return p;
}

[numthreads(256, 1, 1)]
void CS_Emit(uint id : SV_DispatchThreadID)
{
    if (id > gMaxParticlesNum)
        return;
    
    gParticlesIn[id] = SpawnParticle(id);
}

[numthreads(256, 1, 1)]
void CS_Update(uint id : SV_DispatchThreadID)
{
    if (id >= gMaxParticlesNum)
        return;
    
    Particle p = gParticlesIn[id];
    
    if (p.Age >= p.Lifetime)
    {
        p = SpawnParticle(id);
    }
    
    p.Position += p.Velocity * gDeltaTime;
	
    float3 curlPosition = p.Position * 0.1f;
    float3 curlVelocity = curlNoise3D(curlPosition, 1.0f);
    p.Velocity = curlVelocity * 2;
    
    p.Age += gDeltaTime;
    gParticlesOut[id] = p;
}