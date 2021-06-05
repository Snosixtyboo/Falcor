Texture2D source, motion;
SamplerState sampler;

struct Out {
	float4 reprojected : SV_TARGET0;
}

Out main(float2 uv : TEXCOORD) : SV_TARGET {
  Out out;
  out.reprojected = source.SampleLevel(sampler, uv + motion.SampleLevel(sampler, uv, 0).xy, 0);
  return out;
}
