#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D depthAttachment;

void main()
{
    fragColor = texture(depthAttachment, vec2(uv.x, uv.y));
}