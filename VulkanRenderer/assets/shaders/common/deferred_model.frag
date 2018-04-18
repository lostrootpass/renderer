#version 450
#extension GL_ARB_separate_shader_objects : enable

const int MATERIAL_COUNT = 64;

const uint MATFLAG_DIFFUSEMAP = 0x0001;
const uint MATFLAG_BUMPMAP = 0x0002;
const uint MATFLAG_SPECMAP = 0x0004;
const uint MATFLAG_NORMALMAP = 0x0008;
const uint MATFLAG_PRELIT = 0x0010;
const uint MATFLAG_ALPHAMASK = 0x0020;

const uint TEXLAYER_DIFFUSE = 0;
const uint TEXLAYER_BUMP = 1;
const uint TEXLAYER_SPEC = 2;
const uint TEXLAYER_ALPHA = 3;

const uint SCENEFLAG_ENABLESHADOWS = 0x0001;
const uint SCENEFLAG_PRELIT = 0x0002;
const uint SCENEFLAG_ENABLEBUMPMAPS = 0x0004;
const uint SCENEFLAG_MAPSPLIT = 0x0008;
const uint SCENEFLAG_SHOWNORMALS = 0x0010;
const uint SCENEFLAG_ENABLESPECMAPS = 0x0020;
const uint SCENEFLAG_ENABLEPCF = 0x0040;

layout(location = 0) in vec2 uv;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 lightVec;
layout(location = 3) in vec3 viewVec;
layout(location = 4) in vec4 shadowCoord;
layout(location = 5) flat in uint materialId;

layout(location = 0) out uvec4 fragColor;
layout(location = 1) out vec4 normalColor;

layout(set = 2, binding = 0) uniform sampler texsampler;
layout(set = 3, binding = 0) uniform texture2DArray materials[MATERIAL_COUNT];
layout(set = 4, binding = 0) uniform LightData {
    mat4 mvp;
    vec4 color;
    vec3 pos;
} lightData;
layout(set = 5, binding = 0) uniform texture2D shadowMap;

layout(std140, set = 6, binding = 0) uniform MaterialData {
    vec4 ambient[MATERIAL_COUNT];
    vec4 diffuse[MATERIAL_COUNT];
    vec4 specular[MATERIAL_COUNT];
    vec4 emissive[MATERIAL_COUNT];
    vec4 transparency[MATERIAL_COUNT];
    uint flags[MATERIAL_COUNT];
    float shininess[MATERIAL_COUNT];
} materialData;

layout(push_constant) uniform SceneFlags {
    uint flags;
} sceneFlags;

//TODO: pass this in with material data.
const float bumpMapIntensity = 1.0;

bool flag(uint set, uint mask)
{
    return (set & mask) == mask;
}

bool sceneFlag(uint mask)
{
    return flag(sceneFlags.flags, mask);
}

bool matFlag(uint mask)
{
    return flag(materialData.flags[materialId], mask);
}

void main() {
    //Quick check to see if we should just discard and move on.
    if(matFlag(MATFLAG_ALPHAMASK))
    {
        float alpha = texture(sampler2DArray(materials[materialId], texsampler), vec3(uv, TEXLAYER_ALPHA)).r;

        if(alpha < 0.1)
            discard;
    }


    vec3 bump = vec3(0.5);
    if(matFlag(MATFLAG_BUMPMAP) && sceneFlag(SCENEFLAG_ENABLEBUMPMAPS))
    {
        bump = texture(sampler2DArray(materials[materialId], texsampler), vec3(uv, TEXLAYER_BUMP)).rgb;
    }

    vec3 adjustedNormal = normal;
    adjustedNormal += ((bump - 0.5) * bumpMapIntensity);
	normalColor = vec4(adjustedNormal, 1.0);
	
	fragColor.r = uint(fract(uv.x) * 1000.0);
	fragColor.g = uint(fract(uv.y) * 1000.0);
	fragColor.b = materialId;
	fragColor.a = 1;
}
