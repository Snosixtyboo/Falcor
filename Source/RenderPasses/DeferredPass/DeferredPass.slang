import Utils.Color.ColorHelpers;
import Scene.Lights.LightData;
import Scene.Lights.LightProbeData;
import Scene.Shading;
import Utils.Helpers;

cbuffer constants {
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
	float4 viewNormal : SV_TARGET1;
};

PsOut main(float2 texC : TEXCOORD) : SV_TARGET {
	ShadingData sd;
	sd.emissive = gEmissive.SampleLevel(gSampler, texC, 0).xyz;
	sd.posW = gPosW.SampleLevel(gSampler, texC, 0).xyz;

	if (sd.posW.x == 0 && sd.posW.y == 0 && sd.posW.z == 0) {
		discard;
	}

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

	float4 color = float4(0, 0, 0, 1);
	float vis = gVisibility.SampleLevel(gSampler, texC, 0).r;

	for (uint l = 0; l < lightCount; l++) {
		float shadowFactor = 1;
	  if (l == 0) {
			shadowFactor = vis;
			shadowFactor *= sd.opacity;
		}

		color.rgb += evalMaterial(sd, lights[l], shadowFactor).color;
	}

	color.a = sd.opacity;
	color.rgb += sd.emissive;
	color.rgb += evalMaterial(sd, tScene.lightProbe).color;

	PsOut psOut;
	psOut.color = color;
	return psOut;
}
