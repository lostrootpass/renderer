#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../shadercommon.inc"

layout(location = 0) in vec2 uv;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 lightVec;
layout(location = 3) in vec3 viewVec;
layout(location = 4) in vec4 shadowCoord;
layout(location = 5) flat in uint materialId;

layout(location = 0) out uvec4 fragColor;
layout(location = 1) out vec4 normalColor;

layout(set = 2, binding = 0) uniform sampler texsampler;
layout(set = 3, binding = 0) uniform texture2DArray materials[MATERIAL_COUNT];
layout(set = 4, binding = 0) uniform LightUniform {
	LightData lightData;
};
layout(set = 5, binding = 0) uniform texture2D shadowCube;

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

void main() {
    //Quick check to see if we should just discard and move on.
    if(matFlag(MATFLAG_ALPHAMASK))
    {
        float alpha = texture(sampler2DArray(materials[materialId], texsampler), vec3(uv, TEXLAYER_ALPHA)).r;

        if(alpha < 0.1)
            discard;
    }


    vec3 bump = vec3(0.5);
    if(matFlag(MATFLAG_BUMPMAP) && sceneFlag(SCENEFLAG_ENABLEBUMPMAPS))
    {
        bump = texture(sampler2DArray(materials[materialId], texsampler), vec3(uv, TEXLAYER_BUMP)).rgb;
    }

    vec3 adjustedNormal = normal;
    adjustedNormal += ((bump - 0.5) * bumpMapIntensity);
	adjustedNormal = normalize(adjustedNormal) * 0.5 + 0.5;
	normalColor = vec4(adjustedNormal, 1.0);
	
	fragColor.r = uint(fract(uv.x) * 1000.0);
	fragColor.g = uint(fract(uv.y) * 1000.0);
	fragColor.b = materialId;
	fragColor.a = 1;
}
