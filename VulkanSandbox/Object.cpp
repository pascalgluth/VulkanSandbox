#include "Object.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <glm/gtc/type_ptr.hpp>

#include "MaterialManager.h"

Object::Object()
{
    m_transform = glm::mat4(1.f);
    m_position = { 0.f, 0.f, 0.f };

}

Object::~Object()
{
}

void Object::Init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue transferQueue,
    VkCommandPool transferCommandPool, const std::string& modelFile)
{
    m_device = device;
    m_physicalDevice = physicalDevice;

    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(modelFile, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);
    if (!scene) throw std::runtime_error("Failed to load Model: " + modelFile);

    for (size_t i = 0; i < scene->mNumMaterials; ++i)
    {
        aiMaterial* material = scene->mMaterials[i];

        if (material->GetTextureCount(aiTextureType_DIFFUSE))
        {
            aiString path;
            if (material->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS)
            {
                std::string fileName = path.data;
                int idx = fileName.rfind("\\");
                if (idx != std::string::npos) fileName = fileName.substr(idx+1);
                
                m_materialIndices.push_back(MaterialManager::CreateTexture(fileName, transferQueue, transferCommandPool));
            }
        }
        
    }

    m_meshes = LoadNode(transferQueue, transferCommandPool, scene->mRootNode, scene, glm::mat4(1.f));
}

void Object::Update(float deltaTime)
{
}

void Object::Destroy()
{
    for (auto& mesh : m_meshes)
    {
        mesh.Destroy();
    }
}

std::vector<Mesh>& Object::GetMeshes()
{
    return m_meshes;
}

const glm::mat4& Object::GetTransform()
{
    return m_transform;
}

const glm::vec3& Object::GetPosition()
{
    return m_position;
}

void Object::SetPosition(const glm::vec3& newPos)
{
    m_position = newPos;

    m_transform = glm::translate(glm::mat4(1.f), m_position);
}

std::vector<Mesh> Object::LoadNode(VkQueue transferQueue, VkCommandPool transferCommandPool, aiNode* node,
                                   const aiScene* scene, const glm::mat4 parentTransform)
{
    glm::mat4 nodeTransform = glm::transpose(glm::make_mat4(&node->mTransformation.a1)) * parentTransform;
    std::vector<Mesh> meshes = {};
    
    for (size_t i = 0; i < node->mNumMeshes; ++i)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.emplace_back(LoadMesh(transferQueue, transferCommandPool, mesh, scene, nodeTransform));
    }

    for (size_t i = 0; i < node->mNumChildren; ++i)
    {
        std::vector<Mesh> childMeshes = LoadNode(transferQueue, transferCommandPool, node->mChildren[i], scene, nodeTransform);
        meshes.insert(meshes.end(), childMeshes.begin(), childMeshes.end());
    }

    return meshes;
}

Mesh Object::LoadMesh(VkQueue transferQueue, VkCommandPool transferCommandPool, aiMesh* mesh, const aiScene* scene,
    const glm::mat4 parentTransform)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (size_t i = 0; i < mesh->mNumVertices; ++i)
    {
        Vertex v = {};

        v.position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

        if (mesh->mTextureCoords[0])
        {
            v.textureCoord.x = mesh->mTextureCoords[0][i].x;
            v.textureCoord.y = mesh->mTextureCoords[0][i].y;
        }
        
        vertices.push_back(std::move(v));
    }

    for (size_t i = 0; i < mesh->mNumFaces; ++i)
    {
        aiFace face = mesh->mFaces[i];
        for (size_t j = 0; j < face.mNumIndices; ++j)
        {
            indices.push_back(face.mIndices[j]);
        }
    }

    if (mesh->mMaterialIndex >= m_materialIndices.size())
        return { m_device, m_physicalDevice, transferQueue, transferCommandPool, vertices, indices, parentTransform, 0 };
    
    return { m_device, m_physicalDevice, transferQueue, transferCommandPool, vertices, indices, parentTransform, m_materialIndices[mesh->mMaterialIndex] };
}
