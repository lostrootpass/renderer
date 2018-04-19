#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../shadercommon.inc"

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform usampler2D colorAttachment;
layout(set = 0, binding = 1) uniform sampler2D normalAttachment;
layout(set = 0, binding = 2) uniform sampler2D depthAttachment;
layout(set = 1, binding = 0) uniform sampler texsampler;
layout(set = 2, binding = 0) uniform LightUniform {
	LightData lightData;
};
layout(set = 3, binding = 0) uniform texture2DArray materials[MATERIAL_COUNT];
layout(set = 4, binding = 0) uniform CameraUniform {
	Camera camera;
};
layout(set = 5, binding = 0) uniform texture2D shadowMap;
layout(std140, set = 6, binding = 0) uniform MaterialUniform {
	MaterialData materialData;
};
layout(push_constant) uniform SceneFlags {
    uint flags;
} sceneFlags;

bool sceneFlag(uint mask)
{
    return flag(sceneFlags.flags, mask);
}

bool matFlag(uint materialId, uint mask)
{
    return flag(materialData.flags[materialId], mask);
}

float sampleShadowMap(vec3 shadowPos, vec2 offset)
{
    const float SHADOW_BIAS = 0.0005;
	//vec4 coord = shadowCoord/ shadowCoord.w;
    vec4 shadow = texture(sampler2D(shadowMap, texsampler), shadowPos.xy + offset);

    if(shadowPos.z > shadow.r + SHADOW_BIAS)
        return 0.3;
    else
        return 1.0;
}

float shadowPCF(vec3 shadowPos)
{
    float shadowValue = 0.0;
    const float SHADOW_DIM = 1024.0;
    const int SAMPLE_COUNT = 4;

    for(int x = -(SAMPLE_COUNT/2); x < (SAMPLE_COUNT/2); x++)
    {
        for(int y = -(SAMPLE_COUNT/2); y < (SAMPLE_COUNT/2); y++)
        {
            vec2 offset = vec2(x * (1.0/SHADOW_DIM), y * (1.0/SHADOW_DIM));
            shadowValue += sampleShadowMap(shadowPos, offset);
        }
    }

    shadowValue /= (SAMPLE_COUNT*SAMPLE_COUNT);
    return shadowValue;
}

void main()
{
    uvec4 diffuse = texture(colorAttachment, uv);
	if(diffuse.w != 1)
		discard;

    vec4 normal = texture(normalAttachment, uv);
    float depth = texture(depthAttachment, uv).x;
    uint materialId = diffuse.z;

    vec4 ambientMat = materialData.ambient[materialId];
    vec4 diffuseMat = materialData.diffuse[materialId];
    vec4 specularMat = materialData.specular[materialId];
	vec4 emissiveMat = materialData.emissive[materialId];
    vec4 transparencyMat = materialData.transparency[materialId];

    vec2 matUV = vec2((diffuse.x) / 1000.0, (diffuse.y) / 1000.0);
    vec4 albedo = diffuseMat;
	if(matFlag(materialId, MATFLAG_DIFFUSEMAP))
		albedo = texture(sampler2DArray(materials[materialId], texsampler), vec3(matUV, TEXLAYER_DIFFUSE));

	vec4 normalMap = vec4(1.0);
	if(matFlag(materialId, MATFLAG_NORMALMAP))
		normalMap = texture(sampler2DArray(materials[materialId], texsampler), vec3(matUV, TEXLAYER_BUMP));

    float exponent = materialData.specular[materialId].x;
    if(matFlag(materialId, MATFLAG_SPECMAP))
		exponent = texture(sampler2DArray(materials[materialId], texsampler), vec3(matUV, TEXLAYER_SPEC)).r;
	
	float specMul = 1.0;
    if(materialData.shininess[materialId] > 0.0)
		specMul = materialData.shininess[materialId];

	vec4 screenSpace = vec4(uv * 2.0 - 1.0, depth, 1.0);
	vec4 worldSpace = inverse(camera.projview) * screenSpace;
	vec3 worldPos = worldSpace.xyz/worldSpace.w;

	vec4 shadowCoord = biasMatrix * lightData.mvp * vec4(worldPos, 1.0);
	vec3 shadowPos = shadowCoord.xyz / shadowCoord.w;
	vec2 shadowUV = vec2(shadowPos.xy);
	vec4 shadowRGBA = texture(sampler2D(shadowMap, texsampler), shadowUV);
	float shadowDepth = shadowRGBA.r;

	float shadowValue = 1.0;
	if(sceneFlag(SCENEFLAG_ENABLESHADOWS))
	{
		if(sceneFlag(SCENEFLAG_ENABLEPCF))
			shadowValue = shadowPCF(shadowPos);
		else
			shadowValue = sampleShadowMap(shadowPos, vec2(0.0));
	}

	vec3 diffuseValue = albedo.xyz;
	vec3 ambientValue = diffuseValue * 0.2;

	vec3 lightVec = normalize(lightData.pos - worldPos);
	vec3 viewVec = normalize(camera.pos.xyz - worldPos);
    float specAngle = max(0.0, dot(normalize(reflect(-lightVec, normal.xyz)), normalize(viewVec)));
	vec3 specComponent = ambientValue * max(0.0, pow(specAngle * specMul, exponent));

	float dotProd = dot(normalize(lightVec), normal.xyz);
	float clamped = clamp(dotProd, 0.0, 1.0);

	vec3 color = ambientValue + emissiveMat.xyz + specComponent;
	vec3 lighting = diffuseValue * (lightData.color.rgb * clamped);
	fragColor.xyz = color + lighting;
	fragColor.w = 1.0;

	fragColor.xyz *= shadowValue;
}