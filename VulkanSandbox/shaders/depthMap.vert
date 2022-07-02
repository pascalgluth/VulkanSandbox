#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;

layout(binding = 0) uniform UboViewProjection
{
    mat4 view;
    mat4 projection;
    vec4 camPos;
    mat4 lightSpace;
} uboVP;

layout(push_constant) uniform PushModelTransform
{
    mat4 model;
} pushModel;

void main()
{
    gl_Position = uboVP.projection * uboVP.view * pushModel.model * vec4(inPosition, 1.0);
}