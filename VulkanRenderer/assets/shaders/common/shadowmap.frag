#version 450
#extension GL_ARB_separate_shader_objects : enable

const uint MATERIAL_COUNT = 64;

const uint MATFLAG_ALPHAMASK = 0x0020;

layout(location = 0) in vec2 uv;
layout(location = 1) flat in uint materialId;

layout(set = 2, binding = 0) uniform sampler texsampler;
layout(set = 3, binding = 0) uniform texture2DArray materials[MATERIAL_COUNT];
layout(std140, set = 6, binding = 0) uniform MaterialData {
    vec4 ambient[MATERIAL_COUNT];
    vec4 diffuse[MATERIAL_COUNT];
    vec4 specular[MATERIAL_COUNT];
    vec4 emissive[MATERIAL_COUNT];
    vec4 transparency[MATERIAL_COUNT];
    uint flags[MATERIAL_COUNT];
    float shininess[MATERIAL_COUNT];
} materialData;

bool flag(uint set, uint mask)
{
    return (set & mask) == mask;
}

bool matFlag(uint mask)
{
    return flag(materialData.flags[materialId], mask);
}

void main() {
    if(matFlag(MATFLAG_ALPHAMASK))
    {
        float alpha = texture(sampler2DArray(materials[materialId], texsampler), vec3(uv, 3)).r;

        if(alpha < 0.1)
            discard;
    }

    //gl_FragDepth gets written implicitly.
}