cbuffer constant { uint2 resolution; }
Texture2D<float4> reproject;
Texture2D<float3> diffuse;
Texture2D<float3> normals;
RWTexture2D<float> buffer;

[numthreads(32, 32, 1)]
void main(uint3 pixel : SV_DispatchThreadID)
{
    uint2 uv = pixel.xy;
    if (any(uv > resolution)) return;

}
