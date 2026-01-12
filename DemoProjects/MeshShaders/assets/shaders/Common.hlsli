#include "Buffers.hlsli"

bool IsVisible(Instance instance)
{
    float3 center = instance.BoundingSphere.xyz;

    [unroll]
    for (int i = 0; i < 6; i++)
    {
        if (dot(gPlanes[i], float4(center, 1)) < -instance.BoundingSphere.w)
            return false;
    }
    
    return true;
}

bool IsVisible(CullData cullData, float4x4 world)
{
    if ((gFlags & PASS_FLAG_MESHLET_CULLING) == 0)
        return true;
    
    float3 center = mul(float4(cullData.SphereCenter, 1), world);
    
    [unroll]
    for (int i = 0; i < 6; i++)
    {
        if (dot(gPlanes[i], float4(center, 1)) < -cullData.SphereRadius * gScale)
            return false;
    }
    
    int b0 = (cullData.ConeAxisCutoffPacked >> 0) & 0xFF;
    int b1 = (cullData.ConeAxisCutoffPacked >> 8) & 0xFF;
    int b2 = (cullData.ConeAxisCutoffPacked >> 16) & 0xFF;
    int b3 = (cullData.ConeAxisCutoffPacked >> 24) & 0xFF;
    
    b0 = (b0 << 24) >> 24;
    b1 = (b1 << 24) >> 24;
    b2 = (b2 << 24) >> 24;
    b3 = (b3 << 24) >> 24;
    
    float3 coneAxis = float3(b0 / 127.0f, b1 / 127.0f, b2 / 127.0f);
    coneAxis = mul(float4(coneAxis, 0), world);
    
    float3 coneApex = mul(float4(cullData.ConeApex, 1), world).xyz;
    float cutoffAngle = b3 / 127.0f;
    
    if (dot(coneAxis, normalize(coneApex - gCameraPos)) >= cutoffAngle)
        return false;
    
    return true;
}

uint ComputeLod(float4 boundingSphere)
{
    float3 d = boundingSphere.xyz - gCameraPos;
    float r = boundingSphere.w; 
    
    //
    //      0
    //     /|
    //    / |
    // d /  | r 
    //  /   |
    //  ----*
    //    t
    //
    // 0 - sphere center
    // r - sphere radius
    // d - distance from camera to sphere center
    // * - point of tangency
    // t - tangent to the sphere
    
    float t = sqrt(dot(d, d) - r * r);
    
    // tan(a) = r / t
    // size = tan(a) / tan(fovY * 0.5)
    // gInvTanHalfFovY = 1 / tan(fovY * 0.5)
    
    float size = gInvTanHalfFovY * r / t;
    size = min(size, 1.0f);
    
    return round((1.0 - size) * (gLodCount - 1));
}

float3 HashColor(uint id)
{
    id ^= id >> 16;
    id *= 0x7feb352d;
    id ^= id >> 15;
    id *= 0x846ca68b;
    id ^= id >> 16;

    return float3(
        (id & 255) / 255.0,
        ((id >> 8) & 255) / 255.0,
        ((id >> 16) & 255) / 255.0
    );
}

float3 HSVtoRGB(float3 hsv)
{
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(hsv.xxx + K.xyz) * 6.0 - K.www);
    return hsv.z * lerp(K.xxx, saturate(p - K.xxx), hsv.y);
}

float3 LODColorHSV(uint lod, uint maxLod)
{
    float h = (float)lod / maxLod;
    return HSVtoRGB(float3(h, 1.0, 1.0));
}