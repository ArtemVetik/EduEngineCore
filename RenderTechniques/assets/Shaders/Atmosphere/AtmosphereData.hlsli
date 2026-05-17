SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer cbLUTSizes : register(b0)
{
    uint gLutWidth;
    uint gLutHeight;
}

cbuffer cbAtmosphereParameters : register(b1)
{
    float gPlanetRadius;
    float gAtmosphereRadius;
    
    float gMieG;
    uint gPadding0;

    float3 gRayleighScatteringCoefficient;
    float gRayleighScaleHeight;
    float3 gRayleighAbsorptionCoefficient;
    uint gPadding1;
    
    float3 gMieScatteringCoefficient;
    float gMieScaleHeight;
    float3 gMieAbsorptionCoefficient;
    uint gPadding2;
    
    float3 gOzoneScatteringCoefficient;
    uint gPadding3;
    float3 gOzoneAbsorptionCoefficient;
    uint gPadding4;

    float3 gGroundSpectrumAlbedo;
    uint gPadding5;
}

cbuffer cbPass : register(b2)
{
    float3 gSunDirection;
}