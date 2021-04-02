Texture2D buffer, motion;
SamplerState sampler;

struct Out {
	float4 reprojected : SV_TARGET0;
}

Out main(float2 uv : TEXCOORD) : SV_TARGET {
  Out out;
  out.reprojected = buffer.SampleLevel(sampler, uv + motion[uv].xy, 0);
  return out;
}
