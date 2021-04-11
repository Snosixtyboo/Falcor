cbuffer constant { uint2 resolution; }
Texture2D<float3> input;
RWTexture2D<uint> rate;

#define NUM_SAMPLES (float) (VRS_TILE * VRS_TILE)
float luma(uint2 uv) {
  float3 color = input[uv];
  return 0.299*color.r + 0.587*color.g + 0.114*color.b;
}

[numthreads(32, 32, 1)]
void main(uint3 tile : SV_DispatchThreadID)
{
    if (any(tile.xy > resolution)) return;

    uint2 uv = tile.xy * VRS_TILE;
    float2 diff = (0.0, 0.0);
    float sum = 0;

    for (uint u = 0; u<VRS_TILE; u++) {
      for (uint v = 0; v<VRS_TILE; v++) {
        float a = luma(uv);
        float b = luma(uv + uint2(1,0));
        float c = luma(uv + uint2(0,1));

        float x = abs(a - b);
        float y = abs(a - c);

        diff = max(diff, float2(x,y));
        sum += a + b + c;
      }
    }

    float average = sum / (NUM_SAMPLES * 3.0);
    float limit = LIMIT * (average + LUMINANCE);
    float limit2 = limit / 2.13;

    if (diff.x < limit2 && diff.y < limit) {
      rate[tile.xy] = VRS_4x2;
    } else if (diff.x < limit) {
      if (diff.y < limit2) {
        rate[tile.xy] = VRS_2x4;
      } else if (diff.y < limit) {
        rate[tile.xy] = VRS_2x2;
      } else {
        rate[tile.xy] = VRS_2x1;
      }
    } else if (diff.y < limit) {
      rate[tile.xy] = VRS_1x2;
    } else {
      rate[tile.xy] = VRS_1x1;
    }
}
