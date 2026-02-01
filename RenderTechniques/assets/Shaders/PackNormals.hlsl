// based on: https://aras-p.info/texts/CompactNormalStorage.html

//
// Simple: https://aras-p.info/texts/CompactNormalStorage.html#method01xy
//
half2 n_encode_simple(half3 n)
{
    return n.xy * 0.5 + 0.5;
}

half3 n_decode_simple(half2 enc)
{
    half3 n;
    n.xy = enc * 2 - 1;
    n.z = sqrt(1 - dot(n.xy, n.xy));
    return n;
}

// 
// Spherical: https://aras-p.info/texts/CompactNormalStorage.html#method03spherical
//
#define kPI 3.1415926536f
half2 n_encode_spherical(half3 n)
{
    return half4(
      (half2(atan2(n.y, n.x) / kPI, n.z) + 1.0) * 0.5,
      0, 0);
}

half3 n_decode_spherical(half2 enc)
{
    half2 ang = enc * 2 - 1;
    half2 scth;
    sincos(ang.x * kPI, scth.x, scth.y);
    half2 scphi = half2(sqrt(1.0 - ang.y * ang.y), ang.y);
    return half3(scth.y * scphi.x, scth.x * scphi.x, scphi.y);
}

// 
// Spheremap: https://aras-p.info/texts/CompactNormalStorage.html#method04spheremap
//
half2 n_encode_spheremap(half3 n)
{
    half p = sqrt(n.z * 8 + 8);
    return half4(n.xy / p + 0.5, 0, 0);
}

half3 n_decode_spheremap(half2 enc)
{
    half2 fenc = enc * 4 - 2;
    half f = dot(fenc, fenc);
    half g = sqrt(1 - f / 4);
    half3 n;
    n.xy = fenc * g;
    n.z = 1 - f / 2;
    return n;
}

//
// Stereographic: https://aras-p.info/texts/CompactNormalStorage.html#method07stereo
//
half2 n_encode_stereographic(half3 n)
{
    half scale = 1.7777;
    half2 enc = n.xy / (n.z + 1);
    enc /= scale;
    enc = enc * 0.5 + 0.5;
    return half4(enc, 0, 0);
}

half3 n_decode_stereographic(half2 enc)
{
    half scale = 1.7777;
    half3 nn =
        half3(enc, 0) * half3(2 * scale, 2 * scale, 0) +
        half3(-scale, -scale, 1);
    half g = 2.0 / dot(nn.xyz, nn.xyz);
    half3 n;
    n.xy = g * nn.xy;
    n.z = g - 1;
    return n;
}

//
// Wrapper
//
half2 normal_encode(half3 n)
{
#if PACK_NORMALS == 1
    return n_encode_simple(n);
#elif PACK_NORMALS == 2
    return n_encode_spherical(n);
#elif PACK_NORMALS == 3
    return n_encode_spheremap(n);
#elif PACK_NORMALS == 4
    return n_encode_stereographic(n);
#else
    return 0;
#endif
}

half3 normal_decode(half2 enc)
{
#if PACK_NORMALS == 1
    return n_decode_simple(enc);
#elif PACK_NORMALS == 2
    return n_decode_spherical(enc);
#elif PACK_NORMALS == 3
    return n_decode_spheremap(enc);
#elif PACK_NORMALS == 4
    return n_decode_stereographic(enc);
#else
    return 0;
#endif
}