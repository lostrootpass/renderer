#version 450
#extension GL_ARB_separate_shader_objects : enable

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

layout(set = 0, binding = 0) uniform Camera {
    mat4 projview;
    vec4 pos;
} camera;

layout(set = 1, binding = 0) uniform Model {
    mat4 pos;
    float scale;
} model;

layout(set = 4, binding = 0) uniform LightData {
    mat4 mvp;
    vec4 color;
    vec3 pos;
} lightData;

out gl_PerVertex 
{
    vec4 gl_Position;   
};

const mat4 biasMatrix = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);

void main()
{
    vec4 fragPos = model.pos * vec4(inPos * model.scale, 1.0);
    outUV = inUV;
    outNormal = normalize(mat3(model.pos) * inNormal);
    outLightVec = normalize(lightData.pos - fragPos.xyz);
    outViewVec = normalize(camera.pos.xyz - fragPos.xyz);
    outShadowCoord = biasMatrix * lightData.mvp * fragPos;
    outMaterialId = inMaterialId;

    gl_Position =  camera.projview * fragPos;
}