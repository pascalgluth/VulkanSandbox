#version 450

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inWorldPos;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inCamPos;

layout(set = 1, binding = 0) uniform sampler2D textureSampler;
layout(set = 1, binding = 1) uniform sampler2D specularSampler;
layout(set = 1, binding = 2) uniform sampler2D normalSampler;

layout(set = 2, binding = 0) uniform UboPointLight
{
    vec4 position;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
} uboPointLight;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 diffuseColor = texture(textureSampler, inTexCoord);
    vec4 ambientColor = diffuseColor;
    vec4 specularColor = texture(specularSampler, inTexCoord);
    vec3 normal = normalize(texture(normalSampler, inTexCoord).rgb);
    if (normal == vec3(0.0, 0.0, 0.0))
    {
        normal = inNormal;
    }
    
    vec4 ambient = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 diffuse = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 specular = vec4(0.0, 0.0, 0.0, 1.0);
    
    // Point Light
    
    ambient += ambientColor * uboPointLight.ambient;
    
    float distance = length(uboPointLight.position.rgb - inWorldPos);
    //float attenuation = 1.0 / (uboPointLight.constant + uboPointLight.linear * distance + uboPointLight.quadratic * (distance * distance));
    
    vec3 lightDir = normalize(uboPointLight.position.rgb - inWorldPos);
    float diffuseFactor = max(dot(normal, lightDir), 0.0);
    diffuse += diffuseColor * ((uboPointLight.diffuse / (distance / 2)) * diffuseFactor);
    
    vec3 camToFrag = normalize(inCamPos - inWorldPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float specularFactor = pow(max(dot(camToFrag, reflectDir), 0.0), 32);
    specular += specularColor * (uboPointLight.specular * specularFactor);
    
    fragColor = ambient + diffuse + specular;
}