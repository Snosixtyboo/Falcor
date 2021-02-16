Texture2D input2D, motion2D;
SamplerState sampler;

struct Out {
	float4 reproject : SV_TARGET0;
}

Out main(float2 uv : TEXCOORD) : SV_TARGET {
  float2 motion = motion2D.SampleLevel(sampler, uv, 0).rg;
  motion = motion * 2.0f - 1.0f;

  Out out;
  out.reproject = input2D.SampleLevel(sampler, uv + motion, 0);
  return out;
}
