#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(binding = 0) uniform UboViewProjection
{
    mat4 view;
    mat4 projection;
} uboVP;

layout(push_constant) uniform PushModelTransform
{
    mat4 model;
} pushModel;

layout(location = 0) out vec2 outTexCoord;

void main()
{
    gl_Position = uboVP.projection * uboVP.view * pushModel.model * vec4(inPosition, 1.0);
    outTexCoord = inTexCoord;
}