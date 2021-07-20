cbuffer constant { uint2 resolution; }
Texture2D<float3> input;
RWTexture2D<uint> rate;

#define NUM_SAMPLES VRS_TILE * VRS_TILE
float luma(uint2 uv) {
  float3 color = input[uv];
  return 0.299*color.r + 0.587*color.g + 0.114*color.b;
}

[numthreads(32, 32, 1)]
void main(uint3 tile : SV_DispatchThreadID)
{
    uint2 uv = tile.xy;
    if (any(uv > resolution)) return;

    uint2 start = uv * VRS_TILE;
    float2 diff = (0.0, 0.0);
    float sum = 0;

    for (uint u = 0; u<VRS_TILE; u++) {
      for (uint v = 0; v<VRS_TILE; v++) {
        uint2 pixel = start + uint2(u,v);
        float c = luma(pixel + uint2(0,1));
        float b = luma(pixel + uint2(1,0));
        float a = luma(pixel);

        float dx = abs(a - b);
        float dy = abs(a - c);

        diff = max(diff, float2(dx,dy));
        sum += a + b + c;
      }
    }

    float average = sum / (NUM_SAMPLES * 3.0);
    float limit = LIMIT * (average + LUMINANCE);
    float limit2 = limit / 2.13;

    if (diff.x < limit2 && diff.y < limit) {
      rate[uv] = VRS_4x2;
    } else if (diff.x < limit) {
      if (diff.y < limit2) {
        rate[uv] = VRS_2x4;
      } else if (diff.y < limit) {
        rate[uv] = VRS_2x2;
      } else {
        rate[uv] = VRS_2x1;
      }
    } else if (diff.y < limit) {
      rate[uv] = VRS_1x2;
    } else {
      rate[uv] = VRS_1x1;
    }

    rate[uv] = diff.x * 255;
}
