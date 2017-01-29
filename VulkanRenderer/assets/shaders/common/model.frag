#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 color;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec3 lightVec;
layout(location = 4) in vec4 shadowCoord;

layout(location = 0) out vec4 fragColor;

layout(set = 2, binding = 0) uniform sampler texsampler;
layout(set = 3, binding = 0) uniform texture2D diffuse;
layout(set = 4, binding = 0) uniform LightData {
    mat4 mvp;
    vec4 color;
    vec3 pos;
} lightData;
layout(set = 5, binding = 0) uniform texture2D shadowMap;

void main() {
    vec4 coord = shadowCoord/ shadowCoord.w;
    vec4 ambient = lightData.color * 0.2;
    vec4 base = texture(sampler2D(diffuse, texsampler), uv);
    vec4 emissive = vec4(base.xyz * 0.2, 1.0);
    vec4 shadow = texture(sampler2D(shadowMap, texsampler), coord.xy);

    float shadowValue = 1.0;
    if(coord.z > shadow.z)
        shadowValue = 0.0;

    fragColor = emissive + (base * shadowValue * (lightData.color * max(0.0, dot(normalize(lightVec), normal))));
}