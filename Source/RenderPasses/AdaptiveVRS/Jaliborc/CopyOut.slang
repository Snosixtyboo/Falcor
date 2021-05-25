cbuffer constant { uint2 resolution; }
StructuredBuffer<float> metric;
RWTexture2D<float> rate;

[numthreads(32, 32, 1)]
void main(uint3 tile : SV_DispatchThreadID)
{
    uint2 uv = tile.xy;
    if (any(uv > resolution)) return;

    int i = uv.y * resolution.x + uv.x;
    rate[uv] = metric[i]; // change to actual shading rate later
}
