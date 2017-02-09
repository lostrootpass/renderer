#version 450
#extension GL_ARB_separate_shader_objects : enable

const int materialCount = 64;

const uint MATFLAG_DIFFUSEMAP = 0x0001;
const uint MATFLAG_BUMPMAP = 0x0002;
const uint MATFLAG_SPECMAP = 0x0004;
const uint MATFLAG_NORMALMAP = 0x0008;
const uint MATFLAG_PRELIT = 0x0010;

const uint SCENEFLAG_ENABLESHADOWS = 0x0001;
const uint SCENEFLAG_PRELIT = 0x0002;
const uint SCENEFLAG_ENABLEBUMPMAPS = 0x0004;
const uint SCENEFLAG_MAPSPLIT = 0x0008;

layout(location = 0) in vec2 uv;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 lightVec;
layout(location = 3) in vec4 shadowCoord;
layout(location = 4) flat in uint materialId;

layout(location = 0) out vec4 fragColor;

layout(set = 2, binding = 0) uniform sampler texsampler;
layout(set = 3, binding = 0) uniform texture2DArray materials[materialCount];
layout(set = 4, binding = 0) uniform LightData {
    mat4 mvp;
    vec4 color;
    vec3 pos;
} lightData;
layout(set = 5, binding = 0) uniform texture2D shadowMap;

layout(std140, set = 6, binding = 0) uniform MaterialData {
    vec4 ambient[materialCount];
    vec4 diffuse[materialCount];
    vec4 specular[materialCount];
    vec4 emissive[materialCount];
    vec4 transparency[materialCount];
    uint flags[materialCount];
    float shininess[materialCount];
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
    vec4 coord = shadowCoord/ shadowCoord.w;

    vec4 ambient = materialData.ambient[materialId];
    vec4 diffuse = materialData.diffuse[materialId];
    vec4 specular = materialData.specular[materialId];
	vec4 emissive = materialData.emissive[materialId];
    vec4 transparency = materialData.transparency[materialId];

    const bool useBumpMapping = matFlag(MATFLAG_BUMPMAP) && sceneFlag(SCENEFLAG_ENABLEBUMPMAPS);

    if(matFlag(MATFLAG_DIFFUSEMAP))
        diffuse = texture(sampler2DArray(materials[materialId], texsampler), vec3(uv, 0));

    ambient = diffuse * 0.2;

    float bump = 0.5;
    if(useBumpMapping)
    {
        bump = texture(sampler2DArray(materials[materialId], texsampler), vec3(uv, 1)).r;
    }

    if(useBumpMapping && sceneFlag(SCENEFLAG_MAPSPLIT))
    {
        if(uv.x > 0.5)
            diffuse = vec4(bump);
    }
    
    vec4 shadow = texture(sampler2D(shadowMap, texsampler), coord.xy);

    float shadowValue = 1.0;
    if(coord.z > shadow.z && sceneFlag(SCENEFLAG_ENABLESHADOWS))
        shadowValue = 0.3;

    if(sceneFlag(SCENEFLAG_PRELIT) || matFlag(MATFLAG_PRELIT))
    {
        fragColor = diffuse;
    }
    else
    {
        //TODO: improve lighting and fix shading on curved surfaces
        vec3 adjustedNormal = normal;
	    adjustedNormal += ((bump - 0.5) * bumpMapIntensity);

        vec3 color = ambient.xyz + emissive.xyz + (diffuse.xyz * (lightData.color.rgb * clamp(dot(normalize(lightVec), adjustedNormal), 0.0, 1.0)));
        color *= shadowValue;

        fragColor = vec4(color, 1.0);
    }
}