float3x3 InverseTranspose3x3(float3x3 M)
{
    // Note that in HLSL, M_t[0] is the first row, while in GLSL, it is the
    // first column. Luckily, determinant and inverse matrix can be equally
    // defined through both rows and columns.
    float det = dot(cross(M[0], M[1]), M[2]);
    float3x3 adjugate = float3x3(cross(M[1], M[2]),
                                 cross(M[2], M[0]),
                                 cross(M[0], M[1]));
    return adjugate / det;
}

//---------------------------------------------------------------------------------------
// Transforms a normal map sample to world space.
//---------------------------------------------------------------------------------------
float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
	// Uncompress each component from [0,1] to [-1,1].
    float3 normalT = 2.0f * normalMapSample - 1.0f;

	// Build orthonormal basis.
    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);

    float3x3 TBN = float3x3(T, B, N);

	// Transform from tangent space to world space.
    float3 bumpedNormalW = mul(normalT, TBN);

    return bumpedNormalW;
}

float3 TransformNormalToWorldSpace(float3 normalL, float4x4 world)
{
    float3x3 normalTransform = float3x3(world[0].xyz, world[1].xyz, world[2].xyz);
    normalTransform = InverseTranspose3x3(normalTransform);
    
    return normalize(mul(normalL, normalTransform));
}

float4 ReconstructWorldPosFromDepth(float depth, float2 texC, float4x4 invProj, float4x4 invView)
{
    float4 clipSpacePosition = float4(texC * 2 - 1, depth, 1);
    clipSpacePosition.y *= -1.0f;
    float4 viewSpacePosition = mul(clipSpacePosition, invProj);
    viewSpacePosition /= viewSpacePosition.w;
    float4 worldSpacePosition = mul(viewSpacePosition, invView);
    
    return worldSpacePosition;
}