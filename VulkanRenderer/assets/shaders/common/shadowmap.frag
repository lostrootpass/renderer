#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../shadercommon.inc"

layout(location = 0) in vec2 uv;
layout(location = 1) flat in uint materialId;
layout(location = 2) in vec3 fragPos;

layout(location = 0) out float outFragDepth;

layout(set = 2, binding = 0) uniform sampler texsampler;
layout(set = 3, binding = 0) uniform texture2DArray materials[MATERIAL_COUNT];
layout(set = 4, binding = 0) uniform LightUniform { 
	LightData lightData;
};
layout(std140, set = 6, binding = 0) uniform MaterialUniform {
	MaterialData materialData;
};

bool matFlag(uint mask)
{
    return flag(materialData.flags[materialId], mask);
}

void main() {
    if(matFlag(MATFLAG_ALPHAMASK))
    {
        float alpha = texture(sampler2DArray(materials[materialId], texsampler), vec3(uv, 3)).r;

        if(alpha < 0.1)
            discard;
    }

	outFragDepth = length(lightData.pos - fragPos);// / lightData.farPlane;
}
