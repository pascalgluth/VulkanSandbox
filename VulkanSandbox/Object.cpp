#include "Object.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <glm/gtc/type_ptr.hpp>

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
        Vertex v;
        v.position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
        v.color = { 0.5f, 0.5f, 0.5f };
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

    return { m_device, m_physicalDevice, transferQueue, transferCommandPool, vertices, indices, parentTransform };
}
