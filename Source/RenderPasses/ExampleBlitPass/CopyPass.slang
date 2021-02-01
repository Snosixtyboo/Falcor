import Utils.Color.ColorHelpers;
import Scene.Lights.LightData;
import Scene.Lights.LightProbeData;
import Scene.Shading;

cbuffer PerFrameCB
{
	matrix <float, 4, 4> world2View;
	float3 cameraPosition;
	int lightCount;
};

SamplerState gSampler;
Texture2D gPosW;
Texture2D gNormW;
Texture2D gDiffuse;
Texture2D gSpecRough;
Texture2D gEmissive;
Texture2D visibilityBuffer;

StructuredBuffer<LightData> lights;

struct TinyScene {
LightProbeData lightProbe;
float dummy[16];
};

ParameterBlock<TinyScene> tScene;

struct PsOut
{
    float4 color : SV_TARGET0;
    float4 normal : SV_TARGET1;
	float4 viewDir : SV_TARGET2;
	float4 viewNormal : SV_TARGET3;
    float4 roughOpac : SV_TARGET4;
};


PsOut main(float2 texC : TEXCOORD) : SV_TARGET
{
    PsOut psOut;

	float4 diffuseOp = gDiffuse.SampleLevel(gSampler, texC, 0);
	float4 specRough = gSpecRough.SampleLevel(gSampler, texC, 0);
	
	ShadingData sd;
	sd.emissive = gEmissive.SampleLevel(gSampler, texC, 0).xyz;
	sd.posW = gPosW.SampleLevel(gSampler, texC, 0).xyz;

    if (sd.posW.x == 0 && sd.posW.y == 0 && sd.posW.z == 0)
    {
        psOut.color = float4(0,0,0,1);
        psOut.normal = float4(0,0,0,1);
        psOut.viewDir = float4(0,0,0,1);
        psOut.roughOpac = float4(0,0,0,1);
        psOut.viewNormal = float4(0,0,0,1);
        return psOut;
    }

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
    for (uint l = 0; l < lightCount; l++)
    {
		float shadowFactor = 1;
	    if (l == 0)
        {
			shadowFactor = visibilityBuffer.SampleLevel(gSampler, texC, 0).r;
            shadowFactor *= sd.opacity;
        }
        finalColor.rgb += evalMaterial(sd, lights[l], shadowFactor).color;
    }
	
	// Add the emissive component
    finalColor.rgb += sd.emissive;
    finalColor.a = sd.opacity;
    finalColor.rgb += evalMaterial(sd, tScene.lightProbe).color;
	
	psOut.color = finalColor;
	psOut.normal = float4(sd.N * 0.5f + 0.5f, 1.0f);
	if (sd.posW.x == 0 && sd.posW.y == 0 && sd.posW.z == 0)
	{
		psOut.viewDir = float4(0,0,0,0);
	}
	else
	{
		psOut.viewDir = mul(float4(sd.V, 0.0f), world2View);
	}
    psOut.roughOpac = float4(sd.linearRoughness, sd.opacity, 0, 0);
	psOut.viewNormal = mul(float4(sd.N, 0.0f), world2View);
    return psOut;
}

