import Utils.Color.ColorHelpers;
import Scene.Lights.LightData;
import Scene.Lights.LightProbeData;
import Scene.Shading;
import Utils.Helpers;

cbuffer PerFrameCB {
	matrix <float, 4, 4> world2View;
	matrix <float, 4, 4> prevVP;
	matrix <float, 4, 4> currVP;
	float3 cameraPosition;
	int lightCount;
};

SamplerState gSampler;
Texture2D gPosW;
Texture2D gNormW;
Texture2D gDiffuse;
Texture2D gSpecRough;
Texture2D gEmissive;
Texture2D gVisibility;

StructuredBuffer<LightData> lights;
ParameterBlock<TinyScene> tScene;

struct TinyScene {
	LightProbeData lightProbe;
	float dummy[16];
};

struct PsOut {
	float4 color : SV_TARGET0;
	float4 normal : SV_TARGET1;
	float4 viewNormal : SV_TARGET2;
	float4 extra : SV_TARGET3;
	float4 motion : SV_TARGET4;
};


PsOut main(float2 texC : TEXCOORD) : SV_TARGET {
	PsOut psOut;

	ShadingData sd;
	sd.emissive = gEmissive.SampleLevel(gSampler, texC, 0).xyz;
	sd.posW = gPosW.SampleLevel(gSampler, texC, 0).xyz;

  if (sd.posW.x == 0 && sd.posW.y == 0 && sd.posW.z == 0) {
      discard;
  }

  float4 v1 = mul(float4(sd.posW.xyz, 1), prevVP);
  float4 v2 = mul(float4(sd.posW.xyz, 1), currVP);
  float2 prevCrd = (v1.xy / v1.w) * float2(0.5, -0.5);
  float2 currCrd = (v2.xy / v2.w) * float2(0.5, -0.5);
  float2 motion = prevCrd - currCrd;

	float4 diffuseOp = gDiffuse.SampleLevel(gSampler, texC, 0);
	float4 specRough = gSpecRough.SampleLevel(gSampler, texC, 0);

  sd.V = normalize(cameraPosition - sd.posW);
  sd.N = normalize(gNormW.SampleLevel(gSampler, texC, 0).xyz);
	sd.uv = texC;
	sd.diffuse = diffuseOp.rgb;
	sd.opacity = diffuseOp.a;
	sd.NdotV = dot(sd.N, sd.V);
	sd.specular = specRough.rgb;
	sd.linearRoughness = specRough.a;
	sd.ggxAlpha = max(0.0064, sd.linearRoughness * sd.linearRoughness);

	float4 finalColor = float4(0, 0, 0, 1);
	float vis = gVisibility.SampleLevel(gSampler, texC, 0).r;

	for (uint l = 0; l < lightCount; l++) {
		float shadowFactor = 1;
	  if (l == 0) {
			shadowFactor = vis;
			shadowFactor *= sd.opacity;
		}

		finalColor.rgb += evalMaterial(sd, lights[l], shadowFactor).color;
	}

	finalColor.a = sd.opacity;
	finalColor.rgb += sd.emissive;
	finalColor.rgb += evalMaterial(sd, tScene.lightProbe).color;

	psOut.color = finalColor;
	psOut.normal = float4(sd.N * 0.5f + 0.5f, 1.0f);
	psOut.viewNormal = mul(float4(sd.N, 0.0f), world2View);
	psOut.motion = float4(motion * 0.5f + 0.5f, 0, 0);
	psOut.extra = float4(sd.linearRoughness, sd.opacity, vis, 0);

	return psOut;
}
