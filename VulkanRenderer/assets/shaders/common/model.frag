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
layout(set = 3, binding = 1) uniform texture2D bumpmap;
layout(set = 4, binding = 0) uniform LightData {
    mat4 mvp;
    vec4 color;
    vec3 pos;
} lightData;
layout(set = 5, binding = 0) uniform texture2D shadowMap;

//TODO: these should be uniforms.
const bool modelSpaceMapSplit = false;
const bool enableLighting = true;
const bool useBumpMapping = true;
const bool enableShadowing = false;
const float bumpMapIntensity = 0.5;

void main() {
    vec4 coord = shadowCoord/ shadowCoord.w;
    vec4 ambient = lightData.color * 0.2;
    vec4 base = texture(sampler2D(diffuse, texsampler), uv);
    
    float bump = 0.5;
    if(useBumpMapping)
        bump = texture(sampler2D(bumpmap, texsampler), uv).r;

    if(modelSpaceMapSplit && useBumpMapping)
    {
        if(uv.x > 0.5)
            base = vec4(bump);
    }
    
    vec3 emissive = base.xyz * 0.2;
    vec4 shadow = texture(sampler2D(shadowMap, texsampler), coord.xy);

    float shadowValue = 1.0;
    if(coord.z > shadow.z && enableShadowing)
        shadowValue = 0.3;

    if(enableLighting)
    {
        //TODO: improve lighting and fix shading on curved surfaces
        vec3 adjustedNormal = normal;
	    adjustedNormal += ((bump - 0.5) * bumpMapIntensity);

        vec3 color = emissive + (base.xyz * (lightData.color.rgb * clamp(dot(normalize(lightVec), adjustedNormal), 0.0, 1.0)));
        color *= shadowValue;
	    
        fragColor = vec4(color, 1.0);
    }
    else
    {
        fragColor = base;
    }
}