#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outUV;

layout(set = 0, binding = 0) uniform Camera {
    mat4 projview;
} camera;

layout(set = 1, binding = 0) uniform Model {
    mat4 pos;
} model;

out gl_PerVertex 
{
    vec4 gl_Position;   
};

void main()
{
    outColor = inColor;
    outUV = inUV;
    gl_Position = camera.projview * model.pos * vec4(inPos, 1.0);
}