#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 uv;

layout(binding = 0) uniform MVP {
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp;

out gl_PerVertex 
{
    vec4 gl_Position;   
};

void main()
{
    outColor = inColor;
    uv = inUV;
    gl_Position = mvp.proj * mvp.view * mvp.model * vec4(inPos, 1.0);
}