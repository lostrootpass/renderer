#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 color;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec3 lightVec;

layout(location = 0) out vec4 fragColor;

layout(set = 2, binding = 0) uniform sampler texsampler;
layout(set = 3, binding = 0) uniform texture2D diffuse;
layout(set = 4, binding = 0) uniform LightData {
    vec4 color;
    vec3 pos;
} lightData;
// layout(set = 5, binding = 0) uniform MaterialData {
//     vec4 diffuse;
// } material;

void main() {
    //fragColor = vec4(color, 1.0);
    //fragColor = texture(sampler2D(diffuse, texsampler), vec2(uv.x, 1.0 - uv.y));
    //fragColor = texture(sampler2D(diffuse, texsampler), uv);
    vec4 ambient = lightData.color * 0.2;
    vec4 base = texture(sampler2D(diffuse, texsampler), uv);
    vec4 emissive = base * 0.2;
    fragColor = ambient + (base * (lightData.color * max(0.0, dot(normalize(lightVec), normal))));
    //fragColor = (lightData.color * max(0.0, dot(normalize(lightVec), normal)));
    //fragColor = fragColor * texture(sampler2D(diffuse, texsampler), uv);
}