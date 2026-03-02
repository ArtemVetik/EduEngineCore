// Based on Unreal Engine implementation
// Source: 
//  UE_5.6\Engine\Shaders\Private\HZB.usf
//  UE_5.6\Engine\Shaders\Private\ReductionCommon.ush

#define GROUP_TILE_SIZE 8
#define MAX_MIP_BATCH_SIZE 4

SamplerState gPointClampSampler : register(s1);

cbuffer cbPass : register(b0)
{
    float2 gInvInputTextureSize;
    uint gInputTextureIdx;
    uint gOutputTexture0Idx;
    uint gOutputTexture1Idx;
    uint gOutputTexture2Idx;
    uint gOutputTexture3Idx;
}

groupshared float SharedMinDeviceZ[GROUP_TILE_SIZE * GROUP_TILE_SIZE];

uint SignedRightShift(uint x, const int bitshift)
{
    if (bitshift > 0)
    {
        return x << asuint(bitshift);
    }
    else if (bitshift < 0)
    {
        return x >> asuint(-bitshift);
    }
    return x;
}

// Returns the pixel pos [[0; N[[^2 in a two dimensional tile size of N=2^TileSizeLog2, to
// store at a given SharedArrayId in [[0; N^2[[, so that a following recursive 2x2 pixel
// block reduction stays entirely LDS memory banks coherent.
//
//  For example, for TileSizeLog2=3 (i.e. a tile size of 8x8), the mapping from SharedArrayId to pixel position is as follows:
//
//
//  y/x   0   1   2   3   4   5   6   7
//  --------------------------------------
//  0 |   0  16   4  20   1  17   5  21
//  1 |  32  48  36  52  33  49  37  53
//  2 |   8  24  12  28   9  25  13  29
//  3 |  40  56  44  60  41  57  45  61
//  4 |   2  18   6  22   3  19   7  23
//  5 |  34  50  38  54  35  51  39  55
//  6 |  10  26  14  30  11  27  15  31
//  7 |  42  58  46  62  43  59  47  63

uint2 InitialTilePixelPositionForReduction2x2(const uint TileSizeLog2, uint SharedArrayId)
{
    uint x = 0;
    uint y = 0;
	
    [unroll]
    for (uint i = 0; i < TileSizeLog2; i++)
    {
        const uint DestBitId = TileSizeLog2 - 1 - i;
        const uint DestBitMask = 1u << DestBitId;
        x |= DestBitMask & SignedRightShift(SharedArrayId, int(DestBitId) - int(i * 2 + 0));
        y |= DestBitMask & SignedRightShift(SharedArrayId, int(DestBitId) - int(i * 2 + 1));
    }

    return uint2(x, y);
}

void OutputMipLevel(uint mipLevel, uint2 outputPixelPos, float minDeviceZ)
{
#if DIM_MIP_LEVEL_COUNT >= 2
    if (mipLevel == 1)
    {
        RWTexture2D<float> outputTexture1 = ResourceDescriptorHeap[gOutputTexture1Idx];
        outputTexture1[outputPixelPos] = minDeviceZ;
    }
#endif
#if DIM_MIP_LEVEL_COUNT >= 3
    else if (mipLevel == 2)
    {
        RWTexture2D<float> outputTexture2 = ResourceDescriptorHeap[gOutputTexture2Idx];
        outputTexture2[outputPixelPos] = minDeviceZ;
    }
#endif
#if DIM_MIP_LEVEL_COUNT >= 4
    else if (mipLevel == 3)
    {
        RWTexture2D<float> outputTexture3 = ResourceDescriptorHeap[gOutputTexture3Idx];
        outputTexture3[outputPixelPos] = minDeviceZ;
    }
#endif
}

[numthreads(GROUP_TILE_SIZE, GROUP_TILE_SIZE, 1)]
void CS(
    uint2 groupID : SV_GroupID,
    uint groupIndex : SV_GroupIndex
)
{
    Texture2D<float> inputTexture = ResourceDescriptorHeap[gInputTextureIdx];
    RWTexture2D<float> outputTexture0 = ResourceDescriptorHeap[gOutputTexture0Idx];
    
    uint2 dispatchThreadId = groupID * GROUP_TILE_SIZE + InitialTilePixelPositionForReduction2x2(MAX_MIP_BATCH_SIZE - 1, groupIndex);
    
    float2 uv = (dispatchThreadId + 0.5f) * gInvInputTextureSize;
    float4 samples = inputTexture.GatherRed(gPointClampSampler, uv);
    
    float minDeviceZ = min(min(samples.x, samples.y), min(samples.z, samples.w));
    
    uint2 outputPixelPos = dispatchThreadId;
    outputTexture0[outputPixelPos] = minDeviceZ;
    
    SharedMinDeviceZ[groupIndex] = minDeviceZ;
    
    [unroll]
    for (uint mipLevel = 1; mipLevel < DIM_MIP_LEVEL_COUNT; ++mipLevel)
    {
        uint mipTileSize = uint(GROUP_TILE_SIZE) >> mipLevel;
        uint reduceBankSize = mipTileSize * mipTileSize;
        
        uint laneCount = WaveGetLaneCount();
        
        if ((reduceBankSize << 2u) > laneCount)
        {
            GroupMemoryBarrierWithGroupSync();
        }
        
        if (groupIndex < reduceBankSize)
        {
            float4 mipMinDeviceZ;
            mipMinDeviceZ[0] = minDeviceZ;
            
            [unroll]
            for (uint i = 1; i < 4; i++)
            {
                uint ldsIndex = groupIndex + i * reduceBankSize;
                mipMinDeviceZ[i] = SharedMinDeviceZ[ldsIndex];
            }
            
            minDeviceZ = min(min(mipMinDeviceZ.x, mipMinDeviceZ.y), min(mipMinDeviceZ.z, mipMinDeviceZ.w));
            
            outputPixelPos = outputPixelPos >> 1;
            OutputMipLevel(mipLevel, outputPixelPos, minDeviceZ);
            
            SharedMinDeviceZ[groupIndex] = minDeviceZ;
        }
    }
}