SamplerState ss : register(s0);

Texture2D<float4> buffer : register(t0);

struct Pixel
{
    float4 padding : SV_POSITION;
    float2 pos : TEXCOORD0; // xy pixel coordinate (range: [0, 1] x [0, 1])
};

float4 main(Pixel pixel) : SV_Target
{
    return buffer.Sample(ss, pixel.pos);
}
