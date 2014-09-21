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
Texture2D<float2> bufferUV : register(t1);

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

float4 main(Pixel pixel) : SV_Target
{
    // return .5;

    float y = bufferY.Sample(ss, pixel.pos);

    // Color inversion
    y = 1 - y;

    return y;
}
