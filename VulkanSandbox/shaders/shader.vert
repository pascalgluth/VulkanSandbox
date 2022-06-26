#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(push_constant) uniform PushViewProjection
{
    mat4 view;
    mat4 projection;
} vp;

layout(location = 0) out vec3 outColor;

void main()
{
    gl_Position = vec4(inPosition, 1.0) * vp.view * vp.projection;
    outColor = inColor;
}