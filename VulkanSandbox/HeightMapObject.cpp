#include "HeightMapObject.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "MaterialManager.h"

HeightMapObject::HeightMapObject(const std::string& name)
    : Object(name)
{
}

HeightMapObject::~HeightMapObject()
{
}

void HeightMapObject::Init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue transferQueue,
    VkCommandPool transferCommandPool, const std::string& modelFile)
{
    m_device = device;
    m_physicalDevice = physicalDevice;

    loadHeightMap(transferQueue, transferCommandPool, modelFile);
}

void HeightMapObject::Destroy()
{
    Object::Destroy();
}

void HeightMapObject::loadHeightMap(VkQueue transferQueue, VkCommandPool transferCommandPool,
    const std::string& heightMapFile)
{
    int width, height, channels;
    unsigned char* data = stbi_load(heightMapFile.c_str(), &width, &height, &channels, 0);

    std::vector<Vertex> vertices;
    float yScale = 64.f*100.f / 256.f;
    float yShift = 16.f;
    
    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; ++j)
        {
            // Retrieve texel for tex coord (i, j)
            unsigned char* texel = data + (j + width * i) * channels;
            // Get raw height at coordinate
            unsigned char y = texel[0];

            Vertex v = {};
            
            v.position.x = -height/2.f + i;
            v.position.y = ((int)y * yScale - yShift)*-1.f;
            v.position.z = -width/2.f + j;

            v.textureCoord.x = (1.f/width) * j;
            v.textureCoord.y = (1.f/height) * i;

            v.normal = { 0.f, 1.f, 0.f };
            
            vertices.push_back(std::move(v));            
        }
    }
    
    stbi_image_free(data);

    std::vector<uint32_t> indices;

    for (int i = 0; i < height-1; ++i)
    {
        for (int j = 0; j < width; ++j)
        {
            for (int k = 0; k < 2; ++k)
            {
                indices.push_back(j + width * (i + k));
            }
        }
    }

    m_numStrips = height-1;
    m_numVertsPerStrip = width*2;

    m_meshes.emplace_back(m_device, m_physicalDevice, transferQueue, transferCommandPool, vertices, indices, glm::mat4(1.f), 0);
    
    m_materialIndices.push_back(MaterialManager::CreateMaterial("heightmap-1.png", "", "", transferQueue, transferCommandPool));
}
