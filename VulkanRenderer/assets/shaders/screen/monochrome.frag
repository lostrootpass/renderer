#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D colorAttachment;

void main()
{
    vec4 chroma = texture(colorAttachment, vec2(uv.x, uv.y));
    float value = (chroma.r + chroma.g + chroma.b) / 3.0;
    fragColor = vec4(vec3(value), 1.0);
}