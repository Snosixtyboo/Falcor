cbuffer constant { uint2 resolution; }
RWTexture2D<float> buffer;
Texture2D<float> metric;

[numthreads(32, 32, 1)]
void main(uint3 pixel : SV_DispatchThreadID)
{
    uint2 uv = pixel.xy;
    if (any(uv > resolution)) return;

}
