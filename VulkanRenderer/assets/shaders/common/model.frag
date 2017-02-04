#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 uv;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 lightVec;
layout(location = 3) in vec4 shadowCoord;
layout(location = 4) flat in uint materialId;

layout(location = 0) out vec4 fragColor;

layout(set = 2, binding = 0) uniform sampler texsampler;
layout(set = 3, binding = 0) uniform texture2D diffusemap;
layout(set = 3, binding = 1) uniform texture2D bumpmap;
layout(set = 4, binding = 0) uniform LightData {
    mat4 mvp;
    vec4 color;
    vec3 pos;
} lightData;
layout(set = 5, binding = 0) uniform texture2D shadowMap;

const int materialCount = 64;
layout(std140, set = 6, binding = 0) uniform Material {
    vec4 ambient[materialCount];
    vec4 diffuse[materialCount];
    vec4 specular[materialCount];
    vec4 emissive[materialCount];
    vec4 transparency[materialCount];
    float shininess[materialCount];
} materials;

//TODO: these should be uniforms.
const bool modelSpaceMapSplit = false;
const bool enableLighting = true;
const bool useBumpMapping = true;
const bool enableShadowing = false;
const float bumpMapIntensity = 1.0;

void main() {
    vec4 coord = shadowCoord/ shadowCoord.w;

    vec4 ambient = materials.ambient[materialId];
    vec4 diffuse = materials.diffuse[materialId];
    vec4 specular = materials.specular[materialId];
	vec4 emissive = materials.emissive[materialId];
    vec4 transparency = materials.transparency[materialId];


    diffuse = texture(sampler2D(diffusemap, texsampler), uv);
    ambient = diffuse * 0.2;

    float bump = 0.5;
    if(useBumpMapping)
    {
        bump = texture(sampler2D(bumpmap, texsampler), uv).r;
    }

    if(modelSpaceMapSplit && useBumpMapping)
    {
        if(uv.x > 0.5)
            diffuse = vec4(bump);
    }
    
    vec4 shadow = texture(sampler2D(shadowMap, texsampler), coord.xy);

    float shadowValue = 1.0;
    if(coord.z > shadow.z && enableShadowing)
        shadowValue = 0.3;

    if(enableLighting)
    {
        //TODO: improve lighting and fix shading on curved surfaces
        vec3 adjustedNormal = normal;
	    adjustedNormal += ((bump - 0.5) * bumpMapIntensity);

        // vec3 color = ambient.xyz + emissive.xyz + (diffuse.xyz * (lightData.color.rgb * clamp(dot(normalize(lightVec), adjustedNormal), 0.0, 1.0)));
        // color *= shadowValue;
        vec3 color = ambient.xyz + (diffuse.xyz * (lightData.color.rgb * clamp(dot(normalize(lightVec), adjustedNormal), 0.0, 1.0)));
        color *= shadowValue;
	    
        fragColor = vec4(color, transparency.x);
    }
    else
    {
        fragColor = diffuse;
    }
}