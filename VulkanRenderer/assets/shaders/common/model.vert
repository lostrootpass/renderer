#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../shadercommon.inc"

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in uint inMaterialId;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outLightVec;
layout(location = 3) out vec3 outViewVec;
layout(location = 4) out vec4 outShadowCoord;
layout(location = 5) flat out uint outMaterialId;

layout(set = 0, binding = 0) uniform CameraUniform {
	Camera camera;
};

layout(set = 1, binding = 0) uniform ModelUniform {
	Model model;
};

layout(set = 4, binding = 0) uniform LightUniform {
	LightData lightData;
};

out gl_PerVertex 
{
    vec4 gl_Position;   
};

void main()
{
    vec4 fragPos = model.pos * vec4(inPos * model.scale, 1.0);
    outUV = inUV;
    outNormal = normalize(mat3(model.pos) * inNormal);
    outLightVec = (lightData.pos - fragPos.xyz);
    outViewVec = normalize(camera.pos.xyz - fragPos.xyz);
    outShadowCoord = biasMatrix * lightData.proj * lightData.views[0] * fragPos;
    outMaterialId = inMaterialId;

    gl_Position =  camera.projview * fragPos;
}
