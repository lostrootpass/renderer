#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPos;

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

void main()
{
    vec4 fragPos = model.pos * vec4(inPos * model.scale, 1.0);
    gl_Position = lightData.mvp * fragPos;
}