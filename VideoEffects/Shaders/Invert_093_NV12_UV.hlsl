static const float pi = 3.14159265f;

SamplerState ss : register(s0);

// Expected texture format of the shader:
//  - progressive 
//  - NV12 
//  - Pixel aspect ratio 1x1
//  - Mono
//  - Gamma-corrected (perceptual space) with color primaries BT.709
//  - Range [0, 1] (i.e. [0, 255] in uint8)
Texture2D<float> bufferY : register(t0);
Texture2D<float4> bufferUV : register(t1);

struct Pixel
{
    float4 padding : SV_POSITION;
    float2 pos : TEXCOORD0; // xy pixel coordinate (range: [0, 1] x [0, 1])
};

cbuffer Parameters : register(b0)
{
    float width;    // in pixels
    float height;   // in pixels
    float time;     // in seconds
    float value;
};

// Shader processes two pixels at a time (rg = UV0, ba = UV1)
float4 main(Pixel pixel) : SV_Target
{
    float4 uv = bufferUV.Sample(ss, pixel.pos);

    // Color inversion
    uv = 1 - uv;

    return uv;
}
