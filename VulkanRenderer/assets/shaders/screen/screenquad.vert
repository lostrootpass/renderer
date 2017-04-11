#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec2 outUV;

out gl_PerVertex 
{
    vec4 gl_Position;   
};

vec2 positions[4] = vec2[](
    vec2(-1.0, -1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0)
);

vec2 uv[4] = vec2[] (
    vec2(0.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0)
);

void main()
{
    outUV = uv[gl_VertexIndex];
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}