#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;

layout(binding = 0) uniform UboViewProjection
{
    mat4 view;
    mat4 projection;
    vec4 camPos;
} uboVP;

layout(push_constant) uniform PushModelTransform
{
    mat4 model;
} pushModel;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outWorldPos;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out vec3 outCamPos;

void main()
{
    gl_Position = uboVP.projection * uboVP.view * pushModel.model * vec4(inPosition, 1.0);
    outTexCoord = inTexCoord;
    outWorldPos = vec3(pushModel.model * vec4(inPosition, 1.0));
    outNormal = normalize(vec3(pushModel.model * vec4(inNormal, 1.0)));
    outCamPos = uboVP.camPos.rgb;
}