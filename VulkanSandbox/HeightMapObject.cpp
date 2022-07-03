#include "HeightMapObject.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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
    float yScale = 64.f / 256.f;
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
            v.position.y = (int)y * yScale - yShift;
            v.position.z = -width/2.f + j;
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
}
