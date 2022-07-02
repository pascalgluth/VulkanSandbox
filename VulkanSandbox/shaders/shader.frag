#version 450

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inWorldPos;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inCamPos;
layout(location = 4) in vec4 inShadowCoord;
layout(location = 5) in flat uint inShaded;

layout(set = 1, binding = 0) uniform sampler2D textureSampler;
layout(set = 1, binding = 1) uniform sampler2D specularSampler;
layout(set = 1, binding = 2) uniform sampler2D normalSampler;

layout(set = 2, binding = 0) uniform UboPointLight
{
    vec4 direction;
} uboPointLight;

layout(set = 3, binding = 0) uniform sampler2D shadowMap;

layout(location = 0) out vec4 fragColor;

/*float textureProj(vec4 shadowCoord)
{
    float shadow = 1.0;
    if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 )
    {
        float dist = texture( shadowMap, shadowCoord.st).r;
        if ( shadowCoord.w > 0.0 && dist < shadowCoord.z )
        {
            shadow = 0.5;
        }
    }
    return shadow;
}*/

void main()
{
    vec4 diffuseColor = texture(textureSampler, inTexCoord);
    vec4 ambientColor = diffuseColor;
    vec4 specularColor = texture(specularSampler, inTexCoord);
    //vec3 normal = normalize(texture(normalSampler, inTexCoord).rgb);
    //if (normal == vec3(0.0, 0.0, 0.0))
    //{
    //    normal = inNormal;
    //}
    
    //vec4 ambient_ = vec4(0.0, 0.0, 0.0, 1.0);
    //vec4 diffuse = vec4(0.0, 0.0, 0.0, 1.0);
    //vec4 specular = vec4(0.0, 0.0, 0.0, 1.0);
    
    // Point Light
    
    //ambient_ += ambientColor * uboPointLight.ambient_;
    
    //float distance = length(uboPointLight.position.rgb - inWorldPos);
    //float attenuation = 1.0 / (uboPointLight.constant + uboPointLight.linear * distance + uboPointLight.quadratic * (distance * distance));
    
    //vec3 lightDir = normalize(uboPointLight.position.rgb - inWorldPos);
    //vec3 lightDir = normalize(uboPointLight.direction.xyz);
    //float diffuseFactor = max(dot(normal, lightDir), 0.0);
    //diffuse += diffuseColor * ((uboPointLight.diffuse / (dist ance / 2)) * diffuseFactor);
    //diffuse += diffuseColor * uboPointLight.diffuse * diffuseFactor;
    
    //vec3 camToFrag = normalize(inCamPos - inWorldPos);
    //vec3 reflectDir = reflect(-lightDir, normal);
    //float specularFactor = pow(max(dot(camToFrag, reflectDir), 0.0), 32);
    //specular += specularColor * (uboPointLight.specular * specularFactor);

    //float shadow = textureProj(inShadowCoord);
    
    if (inShaded == 0)
    {
        fragColor = diffuseColor;
        return;
    }
    
    //float shadow = 1.0;
    vec4 projCoords = inShadowCoord / inShadowCoord.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    //projCoords.z = 1 - projCoords.z;
    projCoords.y *= -1.0;

    /*if (projCoords.z < 0.99)
    {
        float closestDepth = texture(shadowMap, projCoords.xy).r;
        float currentDepth = projCoords.z;
        float bias = 0.005;
        shadow = closestDepth - bias >= currentDepth ? 1.0 : 0.5;
    }*/

    float shadow = 1.0;
    if ( projCoords.z > -1.0 && projCoords.z < 1.0 )
    {
        float dist = texture(shadowMap, projCoords.xy).r;
        if ( projCoords.w > 0.0 && dist < projCoords.z )
        {
            shadow = 0.5;
        }
    }
    
    vec3 n = normalize(inNormal);
    vec3 l = -normalize(uboPointLight.direction.xyz);
    vec3 diffuse = max(dot(n, l), 0.0) * diffuseColor.xyz;
    
    //fragColor = vec4(diffuse, 1.0);
    fragColor = vec4(diffuse * shadow, 1.0);
    
    //fragColor = texture(shadowMap, projCoords.xy).rgba;
}