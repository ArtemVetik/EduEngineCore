SamplerState gsamPointClamp : register(s0);
SamplerState gsamLinearClamp : register(s1);
SamplerState gsamDepthMap : register(s2);
SamplerState gsamLinearWrap : register(s3);

cbuffer cbPass : register(b0)
{
    float4x4 gView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gProjTex;
};

cbuffer cbConstants : register(b1)
{
    float4 gOffsetVectors[14];
    float4 gBlurWeights[3];
    float2 gInvRenderTargetSize;
    float gOcclusionRadius;
    float gOcclusionFadeStart;
    float gOcclusionFadeEnd;
    float gSurfaceEpsilon;
    
    uint gNormalTexIdx;
    uint gDepthTexIdx;
    uint gRandVectorMapIdx;
    uint gSsaoTex0Idx;
    uint gSsaoTex1Idx;
}

cbuffer cbBlurPass : register(b2)
{
    bool gHorizontalBlur;
}

static const int gSampleCount = 14;
static const int gBlurRadius = 5;

static const float2 pos[3] =
{
    float2(-1, -1),
    float2(-1, +3),
    float2(+3, -1)
};

static const float2 uv[3] =
{
    float2(0, 1),
    float2(0, -1),
    float2(2, 1)
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosV : POSITION;
    float2 TexC : TEXCOORD;
};

#include "PackNormals.hlsli"

VertexOut VS(uint vertexId : SV_VertexID)
{
    VertexOut output;
    output.PosH = float4(pos[vertexId], 0, 1);
    output.TexC = uv[vertexId];
    
    float4 ph = mul(output.PosH, gInvProj);
    output.PosV = ph.xyz / ph.w;
    
    return output;
}

half3 SampleNormal(Texture2D normalMap, float2 texC)
{
#if PACK_NORMALS > 0
    half2 nEnc = normalMap.SampleLevel(gsamPointClamp, texC, 0.0f).xy;
    half3 n = normal_decode(nEnc);
#else
    half3 n = normalMap.SampleLevel(gsamPointClamp, texC, 0.0f).xyz;
#endif
    
    n = mul(n, (float3x3)gView);
    return n;
}

// Determines how much the sample point q occludes the point p as a function
// of distZ.
float OcclusionFunction(float distZ)
{
	//
	// If depth(q) is "behind" depth(p), then q cannot occlude p.  Moreover, if 
	// depth(q) and depth(p) are sufficiently close, then we also assume q cannot
	// occlude p because q needs to be in front of p by Epsilon to occlude p.
	//
	// We use the following function to determine the occlusion.  
	// 
	//
	//       1.0     -------------\
	//               |           |  \
	//               |           |    \
	//               |           |      \
	//               |           |        \
	//               |           |          \
	//               |           |            \
	//  ------|------|-----------|-------------|---------|--> zv
	//        0     Eps          z0            z1        
	//
	
    float occlusion = 0.0f;
    if (distZ > gSurfaceEpsilon)
    {
        float fadeLength = gOcclusionFadeEnd - gOcclusionFadeStart;
		
		// Linearly decrease occlusion from 1 to 0 as distZ goes 
		// from gOcclusionFadeStart to gOcclusionFadeEnd.	
        occlusion = saturate((gOcclusionFadeEnd - distZ) / fadeLength);
    }
	
    return occlusion;
}

float NdcDepthToViewDepth(float z_ndc)
{
    // z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.
    float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
    return viewZ;
}
 
float4 PS_SSAO(VertexOut pin) : SV_Target
{
	// p -- the point we are computing the ambient occlusion for.
	// n -- normal vector at p.
	// q -- a random offset from p.
	// r -- a potential occluder that might occlude p.

    Texture2D normalTex = ResourceDescriptorHeap[gNormalTexIdx];
    Texture2D depthTex = ResourceDescriptorHeap[gDepthTexIdx];
    Texture2D randVectorMap = ResourceDescriptorHeap[gRandVectorMapIdx];
    
	// Get viewspace normal and z-coord of this pixel.  
    half3 n = SampleNormal(normalTex, pin.TexC);
    float pz = depthTex.SampleLevel(gsamDepthMap, pin.TexC, 0.0f).r;
    pz = NdcDepthToViewDepth(pz);

    
	//
	// Reconstruct full view space position (x,y,z).
	// Find t such that p = t*pin.PosV.
	// p.z = t*pin.PosV.z
	// t = p.z / pin.PosV.z
	//
    float3 p = (pz / pin.PosV.z) * pin.PosV;
	
	// Extract random vector and map from [0,1] --> [-1, +1].
    float3 randVec = 2.0f * randVectorMap.SampleLevel(gsamLinearWrap, 4.0f * pin.TexC, 0.0f).rgb - 1.0f;

    float occlusionSum = 0.0f;
	
	// Sample neighboring points about p in the hemisphere oriented by n.
    for (int i = 0; i < gSampleCount; ++i)
    {
		// Are offset vectors are fixed and uniformly distributed (so that our offset vectors
		// do not clump in the same direction).  If we reflect them about a random vector
		// then we get a random uniform distribution of offset vectors.
        float3 offset = reflect(gOffsetVectors[i].xyz, randVec);
	
		// Flip offset vector if it is behind the plane defined by (p, n).
        float flip = sign(dot(offset, n));
		
		// Sample a point near p within the occlusion radius.
        float3 q = p + flip * gOcclusionRadius * offset;
		
		// Project q and generate projective tex-coords.  
        float4 projQ = mul(float4(q, 1.0f), gProjTex);
        projQ /= projQ.w;

		// Find the nearest depth value along the ray from the eye to q (this is not
		// the depth of q, as q is just an arbitrary point near p and might
		// occupy empty space).  To find the nearest depth we look it up in the depthmap.

        float rz = depthTex.SampleLevel(gsamDepthMap, projQ.xy, 0.0f).r;
        rz = NdcDepthToViewDepth(rz);

		// Reconstruct full view space position r = (rx,ry,rz).  We know r
		// lies on the ray of q, so there exists a t such that r = t*q.
		// r.z = t*q.z ==> t = r.z / q.z

        float3 r = (rz / q.z) * q;
		
		//
		// Test whether r occludes p.
		//   * The product dot(n, normalize(r - p)) measures how much in front
		//     of the plane(p,n) the occluder point r is.  The more in front it is, the
		//     more occlusion weight we give it.  This also prevents self shadowing where 
		//     a point r on an angled plane (p,n) could give a false occlusion since they
		//     have different depth values with respect to the eye.
		//   * The weight of the occlusion is scaled based on how far the occluder is from
		//     the point we are computing the occlusion of.  If the occluder r is far away
		//     from p, then it does not occlude it.
		// 
		
        float distZ = p.z - r.z;
        float dp = max(dot(n, normalize(r - p)), 0.0f);

        float occlusion = dp * OcclusionFunction(distZ);

        occlusionSum += occlusion;
    }
	
    occlusionSum /= gSampleCount;
	
    float access = 1.0f - occlusionSum;

	// Sharpen the contrast of the SSAO map to make the SSAO affect more dramatic.
    return saturate(pow(access, 6.0f));
}

float4 PS_Blur(VertexOut pin) : SV_Target
{
    // unpack into float array.
    float blurWeights[12] =
    {
        gBlurWeights[0].x, gBlurWeights[0].y, gBlurWeights[0].z, gBlurWeights[0].w,
        gBlurWeights[1].x, gBlurWeights[1].y, gBlurWeights[1].z, gBlurWeights[1].w,
        gBlurWeights[2].x, gBlurWeights[2].y, gBlurWeights[2].z, gBlurWeights[2].w,
    };

    float2 texOffset;
    Texture2D inputMap;
    
    if (gHorizontalBlur)
    {
        texOffset = float2(gInvRenderTargetSize.x, 0.0f);
        inputMap = ResourceDescriptorHeap[gSsaoTex0Idx];
    }
    else
    {
        texOffset = float2(0.0f, gInvRenderTargetSize.y);
        inputMap = ResourceDescriptorHeap[gSsaoTex1Idx];
    }

    Texture2D depthTex = ResourceDescriptorHeap[gDepthTexIdx];
    Texture2D normalTex = ResourceDescriptorHeap[gNormalTexIdx];
    
	// The center value always contributes to the sum.
    float4 color = blurWeights[gBlurRadius] * inputMap.SampleLevel(gsamPointClamp, pin.TexC, 0.0);
    float totalWeight = blurWeights[gBlurRadius];
	 
    half3 centerNormal = SampleNormal(normalTex, pin.TexC);

    float centerDepth = NdcDepthToViewDepth(depthTex.SampleLevel(gsamDepthMap, pin.TexC, 0.0f).r);

    for (float i = -gBlurRadius; i <= gBlurRadius; ++i)
    {
		// We already added in the center weight.
        if (i == 0)
            continue;

        float2 tex = pin.TexC + i * texOffset;

        half3 neighborNormal = SampleNormal(normalTex, tex);

        float neighborDepth = NdcDepthToViewDepth(depthTex.SampleLevel(gsamDepthMap, tex, 0.0f).r);

		//
		// If the center value and neighbor values differ too much (either in 
		// normal or depth), then we assume we are sampling across a discontinuity.
		// We discard such samples from the blur.
		//
	
        if (dot(neighborNormal, centerNormal) >= 0.8f &&
		    abs(neighborDepth - centerDepth) <= 0.2f)
        {
            float weight = blurWeights[i + gBlurRadius];

			// Add neighbor pixel to blur.
            color += weight * inputMap.SampleLevel(
                gsamPointClamp, tex, 0.0);
		
            totalWeight += weight;
        }
    }

	// Compensate for discarded samples by making total weights sum to 1.
    return color / totalWeight;
}
