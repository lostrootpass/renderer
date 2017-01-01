#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 color;

layout(location = 0) out vec4 fragColor;

layout(location = 0) uniform {
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp;

void main() {
    fragColor = vec4(color, 1.0);
}