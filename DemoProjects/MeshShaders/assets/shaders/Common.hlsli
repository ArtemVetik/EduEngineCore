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