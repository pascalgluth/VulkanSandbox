#version 450

layout(location = 0) in vec2 inTexCoord;

layout(set = 1, binding = 0) uniform sampler2D textureSampler;

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = texture(textureSampler, inTexCoord);
}