cbuffer constant { uint2 resolution; }
Texture2D<uint3> color;
RWTexture2D<uint3> rate;

[numthreads(16, 16, 1)]
void main(uint3 tile : SV_DispatchThreadID)
{
    uint2 pixel = tile.xy * 8;
    if (any(pixel > resolution)) return;

    rate[pixel] = color[pixel];
}
