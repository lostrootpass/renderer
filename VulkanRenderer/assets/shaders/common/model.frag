#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../shadercommon.inc"

layout(location = 0) in vec2 uv;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 lightVec;
layout(location = 3) in vec3 viewVec;
layout(location = 4) in vec4 shadowCoord;
layout(location = 5) flat in uint materialId;

layout(location = 0) out vec4 fragColor;

layout(set = 2, binding = 0) uniform sampler texsampler;
layout(set = 3, binding = 0) uniform texture2DArray materials[MATERIAL_COUNT];
layout(set = 4, binding = 0) uniform LightUniform { 
	LightData lightData;
};
layout(set = 5, binding = 0) uniform textureCube shadowCube;
//layout(set = 5, binding = 1) uniform texture2D shadowMap;
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

bool matFlag(uint mask)
{
    return flag(materialData.flags[materialId], mask);
}

float sampleShadowCube(vec3 offset)
{
	vec3 shadowUV = vec3(-lightVec.x, lightVec.y, -lightVec.z);
    vec4 shadow = texture(samplerCube(shadowCube, texsampler), shadowUV);
	if(length(lightVec) > (shadow.r) + SHADOW_BIAS_CUBE)
        return SHADOW_MUL;
    else
        return 1.0;
}

float shadowCubePCF()
{
	float lightVal = 0.0;
	const float PCF_RADIUS = 0.05;

	for(uint i = 0; i < SHADOW_CUBE_SAMPLES; i++)
	{
		vec3 shadowUV = vec3(-lightVec.x, lightVec.y, -lightVec.z);
		shadowUV += (shadowCubeSampleDirections[i] * PCF_RADIUS);
		float shadow = texture(samplerCube(shadowCube, texsampler), shadowUV).r;

		if(length(lightVec) < shadow + SHADOW_BIAS_CUBE)
			lightVal += 1.0;
	}
	lightVal /= float(SHADOW_CUBE_SAMPLES);
	return max(lightVal, SHADOW_MUL);
}

float sampleShadowMap(vec2 offset)
{
    vec4 coord = shadowCoord/ shadowCoord.w;
	vec2 shadowUV = coord.xy + offset;
	//vec4 shadow = texture(sampler2D(shadowMap, texsampler), shadowUV);
	vec4 shadow = vec4(1.0);
    if(coord.z > shadow.r + SHADOW_BIAS)
        return SHADOW_MUL;
    else
        return 1.0;
}

float shadowPCF()
{
    float shadowValue = 0.0;
    const float SHADOW_DIM = 1024.0;
    const int SAMPLE_COUNT = 4;

    for(int x = -(SAMPLE_COUNT/2); x < (SAMPLE_COUNT/2); x++)
    {
        for(int y = -(SAMPLE_COUNT/2); y < (SAMPLE_COUNT/2); y++)
        {
            vec2 offset = vec2(x * (1.0/SHADOW_DIM), y * (1.0/SHADOW_DIM));
            shadowValue += sampleShadowMap(offset);
        }
    }

    shadowValue /= (SAMPLE_COUNT*SAMPLE_COUNT);
    return shadowValue;
}

void main() {
    vec4 coord = shadowCoord/ shadowCoord.w;

    vec4 ambient = materialData.ambient[materialId];
    vec4 diffuse = materialData.diffuse[materialId];
    vec4 specular = materialData.specular[materialId];
	vec4 emissive = materialData.emissive[materialId];
    vec4 transparency = materialData.transparency[materialId];

    //Quick check to see if we should just discard and move on.
    if(matFlag(MATFLAG_ALPHAMASK))
    {
        float alpha = texture(sampler2DArray(materials[materialId], texsampler), vec3(uv, TEXLAYER_ALPHA)).r;

        if(alpha < 0.1)
            discard;
    }

    const bool useBumpMapping = matFlag(MATFLAG_BUMPMAP) && sceneFlag(SCENEFLAG_ENABLEBUMPMAPS);

    if(matFlag(MATFLAG_DIFFUSEMAP))
        diffuse = texture(sampler2DArray(materials[materialId], texsampler), vec3(uv, TEXLAYER_DIFFUSE));

    ambient = diffuse * 0.2;

    vec3 bump = vec3(0.5);
    if(useBumpMapping)
    {
        bump = texture(sampler2DArray(materials[materialId], texsampler), vec3(uv, TEXLAYER_BUMP)).rgb;
    }

    if(useBumpMapping && sceneFlag(SCENEFLAG_MAPSPLIT))
    {
        if(uv.x > 0.5)
            diffuse = vec4(bump, 1.0);
    }

    float shadowValue = 1.0;
    if(sceneFlag(SCENEFLAG_ENABLESHADOWS))
    {
		if(lightData.numViews == 1)
		{
			if(sceneFlag(SCENEFLAG_ENABLEPCF))
				shadowValue = shadowPCF();
			else
				shadowValue = sampleShadowMap(vec2(0.0));
		}
		else
		{
			if(sceneFlag(SCENEFLAG_ENABLEPCF))
				shadowValue = shadowCubePCF();
			else
				shadowValue = sampleShadowCube(vec3(0.0));
		}
    }

    //TODO: improve lighting and fix shading on curved surfaces
    vec3 adjustedNormal = normal;
    adjustedNormal += ((bump - 0.5) * bumpMapIntensity);

    if(sceneFlag(SCENEFLAG_PRELIT) || matFlag(MATFLAG_PRELIT))
    {
        fragColor = diffuse;
    }
    else if(sceneFlag(SCENEFLAG_SHOWNORMALS))
    {
        fragColor = vec4(adjustedNormal, 1.0);
    }
    else
    {
        vec3 specComponent = vec3(0.0f);
        if(sceneFlag(SCENEFLAG_ENABLESPECMAPS))
        {
            float exponent = materialData.specular[materialId].x;
            if(matFlag(MATFLAG_SPECMAP))
                exponent = texture(sampler2DArray(materials[materialId], texsampler), vec3(uv, TEXLAYER_SPEC)).r;
            
            float mul = 1.0;
            if(materialData.shininess[materialId] > 0.0)
                mul = materialData.shininess[materialId];

            float specAngle = max(0.0, dot(normalize(reflect(-lightVec, adjustedNormal)), normalize(viewVec)));
            specComponent = ambient.xyz * max(0.0, pow(specAngle * mul, exponent));
        }

        vec3 color = ambient.xyz + emissive.xyz + specComponent + (diffuse.xyz * (lightData.color.rgb * clamp(dot(normalize(lightVec), adjustedNormal), 0.0, 1.0)));
        color *= shadowValue;

        fragColor = vec4(color, 1.0);
    }
}
