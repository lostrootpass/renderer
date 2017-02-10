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

layout(location = 0) out vec4 fragColor;

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

float sampleShadowMap(vec2 offset)
{
    const float SHADOW_BIAS = 0.0005;
    vec4 coord = shadowCoord/ shadowCoord.w;
    vec4 shadow = texture(sampler2D(shadowMap, texsampler), coord.xy + offset);

    if(coord.z > shadow.z + SHADOW_BIAS && shadow.w > 0.0)
        return 0.3;
    else
        return 1.0;
}

float shadowPCF()
{
    float shadowValue = 0.0;
    const float SHADOW_DIM = 1024.0;
    const int SAMPLE_COUNT = 4;

    for(int x = -(SAMPLE_COUNT/2); x < (SAMPLE_COUNT/2); x++)
    {
        for(int y = -(SAMPLE_COUNT/2); y < (SAMPLE_COUNT/2); y++)
        {
            vec2 offset = vec2(x * (1.0/SHADOW_DIM), y * (1.0/SHADOW_DIM));
            shadowValue += sampleShadowMap(offset);
        }
    }

    shadowValue /= (SAMPLE_COUNT*SAMPLE_COUNT);
    return shadowValue;
}

void main() {
    vec4 coord = shadowCoord/ shadowCoord.w;

    vec4 ambient = materialData.ambient[materialId];
    vec4 diffuse = materialData.diffuse[materialId];
    vec4 specular = materialData.specular[materialId];
	vec4 emissive = materialData.emissive[materialId];
    vec4 transparency = materialData.transparency[materialId];

    //Quick check to see if we should just discard and move on.
    if(matFlag(MATFLAG_ALPHAMASK))
    {
        float alpha = texture(sampler2DArray(materials[materialId], texsampler), vec3(uv, TEXLAYER_ALPHA)).r;

        if(alpha < 0.1)
            discard;
    }

    const bool useBumpMapping = matFlag(MATFLAG_BUMPMAP) && sceneFlag(SCENEFLAG_ENABLEBUMPMAPS);

    if(matFlag(MATFLAG_DIFFUSEMAP))
        diffuse = texture(sampler2DArray(materials[materialId], texsampler), vec3(uv, TEXLAYER_DIFFUSE));

    ambient = diffuse * 0.2;

    vec3 bump = vec3(0.5);
    if(useBumpMapping)
    {
        bump = texture(sampler2DArray(materials[materialId], texsampler), vec3(uv, TEXLAYER_BUMP)).rgb;
    }

    if(useBumpMapping && sceneFlag(SCENEFLAG_MAPSPLIT))
    {
        if(uv.x > 0.5)
            diffuse = vec4(bump, 1.0);
    }

    float shadowValue = 1.0;
    if(sceneFlag(SCENEFLAG_ENABLESHADOWS))
    {
        if(sceneFlag(SCENEFLAG_ENABLEPCF))
            shadowValue = shadowPCF();
        else
            shadowValue = sampleShadowMap(vec2(0.0, 0.0));
    }

    //TODO: improve lighting and fix shading on curved surfaces
    vec3 adjustedNormal = normal;
    adjustedNormal += ((bump - 0.5) * bumpMapIntensity);

    if(sceneFlag(SCENEFLAG_PRELIT) || matFlag(MATFLAG_PRELIT))
    {
        fragColor = diffuse;
    }
    else if(sceneFlag(SCENEFLAG_SHOWNORMALS))
    {
        fragColor = vec4(adjustedNormal, 1.0);
    }
    else
    {
        vec3 specComponent = vec3(0.0f);
        if(sceneFlag(SCENEFLAG_ENABLESPECMAPS))
        {
            float exponent = materialData.specular[materialId].x;
            if(matFlag(MATFLAG_SPECMAP))
                exponent = texture(sampler2DArray(materials[materialId], texsampler), vec3(uv, TEXLAYER_SPEC)).r;
            
            float mul = 1.0;
            if(materialData.shininess[materialId] > 0.0)
                mul = materialData.shininess[materialId];

            float specAngle = max(0.0, dot(normalize(reflect(-lightVec, adjustedNormal)), normalize(viewVec)));
            specComponent = ambient.xyz * max(0.0, pow(specAngle * mul, exponent));
        }

        vec3 color = ambient.xyz + emissive.xyz + specComponent + (diffuse.xyz * (lightData.color.rgb * clamp(dot(normalize(lightVec), adjustedNormal), 0.0, 1.0)));
        color *= shadowValue;

        fragColor = vec4(color, 1.0);
    }
}