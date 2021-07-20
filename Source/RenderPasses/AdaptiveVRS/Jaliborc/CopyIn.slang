cbuffer constant { uint2 resolution; uint area; }
Texture2D<float4> reproject;
Texture2D<float3> diffuse;
Texture2D<float3> normals;
RWStructuredBuffer<float> input;

[numthreads(32, 32, 1)]
void main(uint3 pixel : SV_DispatchThreadID)
{
    uint2 uv = pixel.xy;
    if (any(uv > resolution)) return;

    int k = uv.y * resolution.x + uv.x;
    input[k] = reproject[uv].r;
    input[area + k] = reproject[uv].a;
    input[area * 2 + k] = diffuse[uv].r;
    input[area * 3 + k] = normals[uv].b;
}
