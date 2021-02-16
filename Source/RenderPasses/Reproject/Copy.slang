SamplerState sampler;
Texture2D input2D;

struct Out {
	float4 copied : SV_TARGET0;
}

Out main(float2 uv : TEXCOORD) : SV_TARGET {
  Out out;
  out.copied = input2D.SampleLevel(sampler, uv, 0);
  return out;
}
