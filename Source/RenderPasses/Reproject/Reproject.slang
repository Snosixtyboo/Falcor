Texture2D input2D, motion2D;
SamplerState sampler;

struct Out {
	float4 reproject : SV_TARGET0;
}

Out main(float2 uv : TEXCOORD) : SV_TARGET {
  Out out;
  out.reproject = input2D.SampleLevel(sampler, uv + motion2D[uv].xy, 0);
  return out;
}
