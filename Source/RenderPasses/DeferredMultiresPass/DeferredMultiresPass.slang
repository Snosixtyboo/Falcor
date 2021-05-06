import Scene.Shading;
import Scene.Lights.LightData;
import Experimental.Scene.Lights.EnvMapLighting;
import Utils.Helpers;
import Utils.Color.ColorHelpers;

cbuffer constants {
	matrix <float, 4, 4> world2View;
	float3 cameraPosition;
	int numLights;
};

SamplerState gSampler;
Texture2D gPosW;
Texture2D gNormW;
Texture2D gDiffuse;
Texture2D gSpecRough;
Texture2D gEmissive;
Texture2D gVisibility;
EnvMapLighting gEnvMapLighting;
StructuredBuffer<LightData> gLights;

struct Output {
	float4 color : SV_TARGET0;
	float4 viewNormal : SV_TARGET1;
};

Output main(float2 uv : TEXCOORD) : SV_TARGET {
	ShadingData material;
	material.emissive = gEmissive.SampleLevel(gSampler, uv, 0).rgb;
	material.posW = gPosW.SampleLevel(gSampler, uv, 0).xyz;

	if (material.posW.x == 0 && material.posW.y == 0 && material.posW.z == 0)
		discard;

	float4 diffuseOp = gDiffuse.SampleLevel(gSampler, uv, 0);
	float4 specRough = gSpecRough.SampleLevel(gSampler, uv, 0);

	material.V = normalize(cameraPosition - material.posW);
	material.N = normalize(gNormW.SampleLevel(gSampler, uv, 0).xyz);
	material.NdotV = dot(material.N, material.V);
	material.diffuse = diffuseOp.rgb;
	material.opacity = diffuseOp.a;
	material.specular = specRough.rgb;
	material.linearRoughness = specRough.a;
	material.ggxAlpha = max(0.0064, material.linearRoughness * material.linearRoughness);
	material.uv = uv;

	float4 color = float4(0, 0, 0, 1);
	float visibility = gVisibility.SampleLevel(gSampler, uv, 0).r * material.opacity;

	for (uint l = 0; l < numLights; l++) {
		float shadowing;
	  if (l == 0)
			shadowing = visibility;
		else
			shadowing = 0;

		color.rgb += evalMaterial(material, gLights[l], shadowing).color;
	}

	color.a = material.opacity;
	color.rgb += material.emissive;
	color.rgb += evalMaterial(material, gEnvMapLighting).color;

	Output out;
	out.color = color;
	out.viewNormal = mul(float4(material.N, 0.0f), world2View);
	return out;
}
