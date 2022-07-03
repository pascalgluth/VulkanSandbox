#pragma once

#include "Object.h"

class HeightMapObject : public Object
{
public:
    HeightMapObject(const std::string& name);
    ~HeightMapObject() override;

    void Init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, const std::string& modelFile) override;
    void Destroy() override;

private:
    uint32_t m_numStrips;
    uint32_t m_numVertsPerStrip;
    
    void loadHeightMap(VkQueue transferQueue, VkCommandPool transferCommandPool, const std::string& heightMapFile);
    
    
};
