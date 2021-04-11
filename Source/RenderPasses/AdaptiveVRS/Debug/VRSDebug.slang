cbuffer constant { uint2 resolution; }
Texture2D<uint> rate;
Texture2D<float3> rendering;
RWTexture2D<float3> color;

[numthreads(32, 32, 1)]
void main(uint3 pixel : SV_DispatchThreadID)
{
    uint2 uv = pixel.xy;
    if (any(uv > resolution)) return;
    uint mode = rate[uv / VRS_TILE];

    if (mode == VRS_1x1) {
      color[uv] = rendering[uv];
    } else {
      float3 overlay;
      if (mode == VRS_4x2 || mode == VRS_2x4)
        overlay = float3(1.0, 0.0, 0.0);
      else if (mode == VRS_2x2)
        overlay = float3(1.0, 1.0, 0.0);
      else if (mode == VRS_2x1 || mode == VRS_1x2)
        overlay = float3(0.0, 1.0, 0.0);

      color[uv] = rendering[uv] * 0.95 + overlay * 0.05;
    }
}
