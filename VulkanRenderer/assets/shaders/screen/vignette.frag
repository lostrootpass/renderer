#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D colorAttachment;

void main()
{
    //TODO: pass in vars somehow
    float intensity = 0.15;
    float vignette = min(1.0, (1.0/intensity) * (sin(uv.x * 3.14) * sin(uv.y * 3.14)));

    vec4 vignetteColor = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 baseColor = texture(colorAttachment, vec2(uv.x, uv.y));

    fragColor = vec4(vignette * (baseColor - vignetteColor) + vignetteColor);
    fragColor.a = 1.0;
}