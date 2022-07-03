#version 450

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inWorldPos;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inCamPos;
layout(location = 4) in vec4 inShadowCoord;
layout(location = 5) in vec4 inSpotLightShadowCoord;
layout(location = 6) in flat uint inShaded;

layout(set = 1, binding = 0) uniform sampler2D textureSampler;
layout(set = 1, binding = 1) uniform sampler2D specularSampler;
layout(set = 1, binding = 2) uniform sampler2D normalSampler;

layout(set = 2, binding = 0) uniform UboLight
{
    vec4 dlDirection;

    vec4 slPosition;
    vec4 slDirection;
    float slStrength;
    float slCutoff;

} uboLight;

layout(set = 3, binding = 0) uniform sampler2D shadowMapDL;
layout(set = 3, binding = 1) uniform sampler2D shadowMapSL;

layout(set = 4, binding = 0) uniform UboFragSettings
{
    uint drawShadowMap;
} fragSettings;

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
    vec3 n = normalize(inNormal);
    //vec3 normal = normalize(texture(normalSampler, inTexCoord).rgb);
    //if (normal == vec3(0.0, 0.0, 0.0))
    //{
    //    normal = inNormal;
    //}

    vec3 diffuse = vec3(0.0, 0.0, 0.0);
    
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
    
    // -- SPOT LIGHT --
    
    float shadow1 = 1.0;
    vec4 projCoordsSL = inSpotLightShadowCoord / inSpotLightShadowCoord.w;
    projCoordsSL.xy = projCoordsSL.xy * 0.5 + 0.5;
    
    vec3 vecToLight = normalize(inWorldPos - uboLight.slPosition.xyz);
    float theta = acos(dot(vecToLight, normalize(uboLight.slDirection.xyz)));
    float thetaDeg = theta * 180 / 3.14159265;
    
    if (thetaDeg < uboLight.slCutoff)
    {
        float edgeIntensity = clamp((uboLight.slCutoff - thetaDeg) / 10, 0.f, 1.f);
        float distance = distance(uboLight.slPosition.xyz, inWorldPos);
        diffuse += ((max(dot(n, -vecToLight), 0.0) * uboLight.slStrength) / distance) * edgeIntensity * diffuseColor.xyz;

        if (projCoordsSL.z < 0.99)
        {
            float bias = 0.005;
            shadow1 = texture(shadowMapSL, projCoordsSL.xy).r >= projCoordsSL.z ? 1.0 : 0.5;
        }
    }
    
    // ----------------
    
    // -- DIRECTIONAL LIGHT --

    float shadow2 = 1.0;
    vec4 projCoordsDL = inShadowCoord / inShadowCoord.w;
    projCoordsDL.xy = projCoordsDL.xy * 0.5 + 0.5;
    
    if (projCoordsDL.z < 0.99)
    {
        float closestDepth = texture(shadowMapDL, projCoordsDL.xy).r;
        float currentDepth = projCoordsDL.z;
        float bias = 0.005;
        shadow2 = closestDepth >= currentDepth - bias ? 1.0 : 0.5;
    }
    
    vec3 l = -normalize(uboLight.dlDirection.xyz);
    diffuse += max(dot(n, l), 0.0) * diffuseColor.xyz;
    
    // ---------------------
    
    if (fragSettings.drawShadowMap == 1)
    {
        fragColor = texture(shadowMapSL, projCoordsSL.xy);
        return;
    }
    
    if (inShaded == 1)
    {
        float shadow;
        if (shadow1 == shadow2) shadow = shadow1;
        if (shadow1 != shadow2) shadow = min(shadow1, shadow2);
        
        fragColor = vec4(diffuse * shadow2, 1.0);
    }
    else
    {
        fragColor = vec4(diffuse, 1.0);
    }
}