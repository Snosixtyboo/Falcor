cbuffer constant { uint2 resolution; }
Texture2D<float3> input;
RWTexture2D<float3> rate;

float luma(uint2 uv) {
  float3 color = input[uv];
  return 0.299*color.r + 0.587*color.g + 0.114*color.b;
}

[numthreads(32, 32, 1)]
void main(uint3 tile : SV_DispatchThreadID)
{
    if (any(tile.xy > resolution)) return;

    uint2 uv = tile.xy * 16;
    float2 diff = (0.0, 0.0);
    float sum = 0;

    for (uint u = 0; u<16; u+=2) {
      for (uint v = 0; v<16; v+=2) {
        float a = luma(uv);
        float b = luma(uv + uint2(1,0));
        float c = luma(uv + uint2(0,1));

        float x = abs(a - b);
        float y = abs(a - c);

        diff = max(diff, float2(x,y));
        sum += a + b + c;
      }
    }

    float luminance = sum / (16.0 * 16.0 * 3.0) + 0.0001;
    float2 weber = diff / luminance;

    if (weber.x < LIMIT) {
      if (weber.y < LIMIT) {
        rate[tile.xy] = VRS_2x2;
      } else {
        rate[tile.xy] = VRS_2x1;
      }
    } else if (weber.y < LIMIT) {
      rate[tile.xy] = VRS_1x2;
    } else {
      rate[tile.xy] = VRS_1x1;
    }

    if (rate[tile.xy].r == VRS_2x2)
      rate[tile.xy] = float3(0.0, 1.0, 0.0);
    else if (rate[tile.xy].r == VRS_1x1)
      rate[tile.xy] = float3(1.0, 0.0, 0.0);
    else if (rate[tile.xy].r == VRS_1x2)
      rate[tile.xy] = float3(0.0, 0.0, 1.0);
    else
      rate[tile.xy] = float3(1.0, 1.0, 0.0);
    //rate[tile.xy] = float3(weber, 0.0);
}
