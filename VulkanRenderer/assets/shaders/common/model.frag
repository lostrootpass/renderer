#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 color;
layout(location = 1) in vec2 uv;

layout(location = 0) out vec4 fragColor;

layout(set = 2, binding = 0) uniform sampler texsampler;
layout(set = 3, binding = 0) uniform texture2D diffuse;

void main() {
    //fragColor = vec4(color, 1.0);
    //fragColor = texture(sampler2D(diffuse, texsampler), vec2(uv.x, 1.0 - uv.y));
    fragColor = texture(sampler2D(diffuse, texsampler), uv);
}