#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../shadercommon.inc"

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform CameraUniform {
	Camera camera;
};
layout(set = 3, binding = 0) uniform sampler2D ssaoTexture;

void main()
{
	const int BLUR_SIZE = 2; // 4x4 noise texture, 2 samples per direction
	const vec2 texelSize = 1.0 / vec2(camera.width, camera.height);

	float blur = 0.0;

	for(int x = -BLUR_SIZE; x < BLUR_SIZE; x++)
	{
		for(int y = -BLUR_SIZE; y < BLUR_SIZE; y++)
		{
			vec2 offset = vec2(x, y) * texelSize;
			blur += texture(ssaoTexture, uv + offset).r;
		}
	}

	fragColor = vec4(blur / 16.0);
}
