#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../shadercommon.inc"
const int SSAO_KERNEL_COUNT = 32;
const float SSAO_RADIUS = 0.005; //0.05
const float SSAO_SAMPLE_BIAS = 0.00025;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform CameraUniform {
	Camera camera;
};
layout(set = 1, binding = 0) uniform usampler2D colorAttachment;
layout(set = 1, binding = 1) uniform sampler2D normalAttachment;
layout(set = 1, binding = 2) uniform sampler2D depthAttachment;
layout(set = 2, binding = 0) uniform sampler2D noiseTexture;
layout(set = 2, binding = 1) uniform Kernel {
	vec4 kernelSamples[SSAO_KERNEL_COUNT];
};

void main()
{
	const vec2 noiseScale = vec2(camera.width / 4.0, camera.height / 4.0);

	float depth = texture(depthAttachment, uv).r;
	
	vec4 screenSpace = vec4(uv * 2.0 - 1.0, depth, 1.0);
	vec4 unproj = camera.invProj * screenSpace;
	vec3 viewSpace = unproj.xyz/unproj.w;

	vec4 normalIn = texture(normalAttachment, uv);
	normalIn = camera.proj * camera.view * normalIn;
	normalIn = normalIn * 0.5 + 0.5;
	vec3 normal = normalIn.rgb;
	vec3 randomVec = texture(noiseTexture, uv * noiseScale).rgb;

	vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent = cross(normal, tangent);
	mat3 tbnMatrix = mat3(tangent, bitangent, normal);
	
	float ssaoVal = 0.0;
	for(int i = 0; i < SSAO_KERNEL_COUNT; i++)
	{
		vec3 sampleViewSpace = tbnMatrix * kernelSamples[i].xyz;
		sampleViewSpace = viewSpace + sampleViewSpace * SSAO_RADIUS;

		vec4 projectedSample = vec4(sampleViewSpace, 1.0);
		projectedSample = camera.proj * projectedSample;
		vec2 ndcSample = projectedSample.xy / projectedSample.w;
		ndcSample = ndcSample * 0.5 + 0.5;

		float sampleDepth = texture(depthAttachment, ndcSample).r;
		float projectedDepth = (projectedSample.z / projectedSample.w);

		float rangeTolerance = smoothstep(0.0, 1.0, SSAO_RADIUS / abs(sampleDepth - projectedDepth));
		if(sampleDepth <= projectedDepth + SSAO_SAMPLE_BIAS)
			ssaoVal += 1.0 * rangeTolerance;
	}

	ssaoVal = (ssaoVal / float(SSAO_KERNEL_COUNT));
	fragColor = vec4(ssaoVal);
}
