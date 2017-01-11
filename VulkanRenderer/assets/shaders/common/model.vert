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

layout(set = 0, binding = 0) uniform Camera {
    mat4 projview;
} camera;

layout(set = 1, binding = 0) uniform Model {
    mat4 pos;
} model;

layout(set = 4, binding = 0) uniform LightData {
    vec4 color;
    vec3 pos;
} lightData;

out gl_PerVertex 
{
    vec4 gl_Position;   
};

void main()
{
    vec4 fragPos = model.pos * vec4(inPos, 1.0);
    outColor = inColor;
    outUV = inUV;
    outNormal = inNormal;// * fragPos.xyz;
    outLightVec = lightData.pos - fragPos.xyz;
    gl_Position = camera.projview * fragPos;
}