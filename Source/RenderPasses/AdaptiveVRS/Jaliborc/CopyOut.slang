cbuffer constant { uint2 resolution; uint area; }
StructuredBuffer<float> metric;
RWTexture2D<uint> rate;

[numthreads(32, 32, 1)]
void main(uint3 tile : SV_DispatchThreadID)
{
    uint2 uv = tile.xy;
    if (any(uv > resolution)) return;

    int k = uv.y * resolution.x + uv.x;
    rate[uv] = metric[k] * 255;
}
