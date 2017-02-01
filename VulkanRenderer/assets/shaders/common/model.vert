#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outUV;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out vec3 outLightVec;
layout(location = 4) out vec4 outShadowCoord;

layout(set = 0, binding = 0) uniform Camera {
    mat4 projview;
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
    outColor = inColor;
    outUV = inUV;
    outNormal = inNormal;
    outLightVec = lightData.pos - fragPos.xyz;
    outShadowCoord = biasMatrix * lightData.mvp * fragPos;

    gl_Position =  camera.projview * fragPos;
}