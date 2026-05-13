static const float PI = 3.14159265;

cbuffer cbConstants : register(b0)
{
    uint gNbCascades;
    uint gTextureSize;
}

cbuffer cbInitSpectrumData : register(b1)
{
    float gWindSpeed;
    float gWindDirectionX;
    float gWindDirectionY;
    float gGravity;
    float gFetch;
    float gDepth;
    
    uint gRandomNoiseTextureIdx;      // Texture2D<float2>
    uint gInitialSpectrumTexturesIdx; // RWTexture2DArray<float4>
    uint gWavesDataTexturesIdx;       // RWTexture2DArray<float4>
    uint gWavelengthsIdx;             // StructuredBuffer<float>
    uint gCutoffsIdx;                 // StructuredBuffer<float>
    uint gFadesIdx;                   // StructuredBuffer<float>
    uint gSwellsIdx;                  // StructuredBuffer<float>
}

// Returns wave's angular frequency given its wave number
// For more info: https://en.wikipedia.org/wiki/Dispersion_(water_waves)
float AngularFrequency(float k)
{
    return sqrt(gGravity * k); // Angular frequency deep water
}

// TMA modifier for a given angular frequency
// See: https://dl.acm.org/doi/10.1145/2791261.2791267 ("5.1.5 The Texel MARSEN ARSLOE (TMA) Spectrum")
float TMACorrection(float angularFrequency)
{
    float angularFrequencyH = angularFrequency * sqrt(gDepth / gGravity);
    if (angularFrequencyH <= 1.0f)
        return 0.5f * angularFrequencyH * angularFrequencyH;
    if (angularFrequencyH < 2.0f)
        return 1.0f - 0.5f * (2.0f - angularFrequencyH) * (2.0f - angularFrequencyH);
    return 1.0f;
}

// Returns JONSWAP spectrum value for a given angular frequency
// https://www.wikiwaves.org/index.php/Ocean-Wave_Spectra
float JONSWAP(float angularFrequency, float peakAngularFrequency)
{
    float alpha = 0.076f * pow(abs(gWindSpeed * gWindSpeed / (gFetch * gGravity)), 0.22f);
    float gamma = 3.3f;
    float sigma = angularFrequency <= peakAngularFrequency ? 0.07f : 0.09f;

    float frequencyMinusPeakFrequency = angularFrequency - peakAngularFrequency;
    float r = exp(-(frequencyMinusPeakFrequency * frequencyMinusPeakFrequency) / (2 * sigma * sigma * peakAngularFrequency * peakAngularFrequency));

    return alpha * gGravity * gGravity / pow(angularFrequency, 5) * exp(-1.25 * pow(peakAngularFrequency / angularFrequency, 4)) * pow(abs(gamma), r);
}

// Returns spread
// https://www.sciencedirect.com/topics/engineering/directional-spreading (fig. 5.63 and 5.64)
float SpreadPower(float angularFrequency, float peakAngularFrequency)
{
    if (angularFrequency < 1.05 * peakAngularFrequency)
        return 6.97 * pow(abs(angularFrequency / peakAngularFrequency), 4.06);

    float peakSpeed = gGravity / peakAngularFrequency;
    float mu = -2.33 - 1.45 * (gWindSpeed / peakSpeed - 1.17);
    return 9.77 * pow(abs(angularFrequency / peakAngularFrequency), mu);
}

// Normalization factor for directional spreading
float NormalizationFactor(float s)
{
    float s2 = s * s;
    float s3 = s2 * s;
    if (s <= 0.4f)
        return 0.09f * s3 + (pow(log(2), 2) / PI - PI / 12.0f) * s2 + log(2) / PI * s + 1 / (2 * PI);
    return sqrt(s) / (2.0f * sqrt(PI)) + 1 / (16.0f * sqrt(PI * s));
}

// Returns directional spread from given frequency and angle
// https://www.sciencedirect.com/topics/engineering/directional-spreading (fig. 5.60)
float DirectionalSpread(float angularFrequency, float peakAngularFrequency, float theta, float swell)
{
    float s = SpreadPower(angularFrequency, peakAngularFrequency) + 16 * tanh(angularFrequency / peakAngularFrequency) * swell * swell;
    float2 normalizedWindDirection = normalize(float2(gWindDirectionX, gWindDirectionY));
    float windTheta = atan2(normalizedWindDirection.y, normalizedWindDirection.x);

    return NormalizationFactor(s) * pow(abs(cos(0.5f * (theta - windTheta))), 2.0f * s);
}

// Derivative of the angular frequency with respect to wave number
float FrequencyDerivative(float k, float angularFrequency)
{
    float th = tanh(min(k * gDepth, 20));
    float ch = cosh(k * gDepth);
    return gGravity * (gDepth * k / ch / ch + th) / (angularFrequency * 2);
}

// Attenuation factor for short waves
// https://www.researchgate.net/publication/264839743_Simulating_Ocean_Water (formula 41)
float ShortWavesFade(float k, float fade)
{
    return exp(-fade * fade * k * k);
}

[numthreads(8, 8, 1)]
void CalculateInitialSpectrumTextures(uint3 id : SV_DispatchThreadID)
{
    Texture2D<float2> randomNoiseTexture = ResourceDescriptorHeap[gRandomNoiseTextureIdx];
    RWTexture2DArray<float4> initialSpectrumTextures = ResourceDescriptorHeap[gInitialSpectrumTexturesIdx];
    RWTexture2DArray<float4> wavesDataTextures = ResourceDescriptorHeap[gWavesDataTexturesIdx];
    StructuredBuffer<float> wavelengths = ResourceDescriptorHeap[gWavelengthsIdx];
    StructuredBuffer<float> cutoffs = ResourceDescriptorHeap[gCutoffsIdx];
    StructuredBuffer<float> swells = ResourceDescriptorHeap[gSwellsIdx];
    StructuredBuffer<float> fades = ResourceDescriptorHeap[gFadesIdx];
    
    uint halfSize = gTextureSize / 2;
    int nx = id.x - halfSize;
    int nz = id.y - halfSize;
    float gaussianRandomNumber1 = randomNoiseTexture[id.xy].x;
    float gaussianRandomNumber2 = randomNoiseTexture[id.xy].y;

    for (uint i = 0; i < gNbCascades; ++i)
    {
        // K is the angular wavenumber
        // https://en.wikipedia.org/wiki/Wavenumber
        float k = 2.0f * PI / wavelengths[i];
        float2 kVector = float2(nx, nz) * k;
        float kMagnitude = length(kVector);

        if (kMagnitude >= cutoffs[i * 2] && kMagnitude <= cutoffs[i * 2 + 1])
        {
            float kAngle = atan2(kVector.y, kVector.x);
            float angularFrequency = AngularFrequency(kMagnitude);
            // The peak frequency is calculated here because we also need it for the directional spread
            float peakAngularFrequency = 22 * pow(abs(gGravity * gGravity / (gWindSpeed * gFetch)), 0.3333f);

            initialSpectrumTextures[uint3(id.xy, i)] = float4(float2(gaussianRandomNumber1, gaussianRandomNumber2) / 2.0f
                * sqrt(2.0f * TMACorrection(angularFrequency)
                * JONSWAP(angularFrequency, peakAngularFrequency)
                * DirectionalSpread(angularFrequency, peakAngularFrequency, kAngle, swells[i])
                * ShortWavesFade(kMagnitude, fades[i])
                * FrequencyDerivative(kMagnitude, angularFrequency) / kMagnitude * k * k), 0.0f, 0.0f);
            
            // WavesData textures contain information of the wave on each pixel
            // float4(direction on the x axis, 1/k for the FFT, direction on the z axis, wave frequency);
            wavesDataTextures[uint3(id.xy, i)] = float4(kVector.x, 1 / kMagnitude, kVector.y, angularFrequency);
        }
        else
        {
            initialSpectrumTextures[uint3(id.xy, i)] = 0;
            wavesDataTextures[uint3(id.xy, i)] = float4(kVector.x, 1, kVector.y, 0);
        }
    }
}

// We store on each pixel the values of the initial amplitude of that pixel and the initial amplitude of the complex conjugated pixel
// This will help us on the time dependent amplitudes calculations
// https://www.researchgate.net/publication/264839743_Simulating_Ocean_Water (formula 43)
// https://www.cg.tuwien.ac.at/research/publications/2018/GAMPER-2018-OSG/GAMPER-2018-OSG-thesis.pdf (Hermitian Wave Spectrum and Complex Conjugate Indices sections)
[numthreads(8, 8, 1)]
void CalculateConjugatedInitialSpectrumTextures(uint3 id : SV_DispatchThreadID)
{
    RWTexture2DArray<float4> initialSpectrumTextures = ResourceDescriptorHeap[gInitialSpectrumTexturesIdx];
    
    for (uint i = 0; i < gNbCascades; ++i)
    {
        float2 H0K = initialSpectrumTextures[uint3(id.xy, i)].xy;
        uint2 complexConjugateIndex = uint2((gTextureSize - id.x) % gTextureSize, (gTextureSize - id.y) % gTextureSize);
        float2 H0MinusK = initialSpectrumTextures[uint3(complexConjugateIndex, i)].xy;
        initialSpectrumTextures[uint3(id.xy, i)] = float4(H0K.x, H0K.y, H0MinusK.x, -H0MinusK.y);
    }
}