#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../shadercommon.inc"

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in uint inMaterialId;

layout(location = 0) out vec2 uv;
layout(location = 1) flat out uint materialId;
layout(location = 2) out vec3 outFragPos;

layout(set = 1, binding = 0) uniform ModelUniform {
	Model model;
};

layout(set = 4, binding = 0) uniform LightUniform {
	LightData lightData;
};

layout(push_constant) uniform FaceData {
	uint face;
};

out gl_PerVertex 
{
    vec4 gl_Position;   
};

void main()
{
    uv = inUV;
    materialId = inMaterialId;
    vec4 fragPos = model.pos * vec4(inPos * model.scale, 1.0);
    gl_Position = lightData.proj * lightData.views[face] * fragPos;
	outFragPos = fragPos.xyz;
}
