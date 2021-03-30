cbuffer constant { uint2 resolution; }
Texture2D<float3> input;
RWTexture2D<float3> rate;

float luma(uint2 uv) {
  float3 color = input[uv];
  return 0.299*color.r + 0.587*color.g + 0.114*color.b;
}

[numthreads(16, 16, 1)]
void main(uint3 tile : SV_DispatchThreadID)
{
    uint2 uv = tile.xy * 8;
    if (any(uv > resolution)) return;

    float sum = 0;
    float2 diff = (0.0, 0.0);

    for (uint u = 0; u<8; u+=2) {
      for (uint v = 0; v<8; v+=2) {
        float a = luma(uv);
        float b = luma(uv + uint2(1,0));
        float c = luma(uv + uint2(0,1));

        float x = abs(a - b);
        float y = abs(a - c);

        diff = max(diff, float2(x,y));
        sum += a + b + c;
      }
    }

    float luminance = sum / (8.0 * 8.0 * 3.0) + 0.0001;
    rate[tile.xy] = float3(diff / luminance, 0.0);
}
