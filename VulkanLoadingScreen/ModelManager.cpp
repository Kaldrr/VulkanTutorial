#include <VulkanLoadingScreen/ModelManager.h>
#include <VulkanLoadingScreen/Vertex.h>
#include <VulkanLoadingScreen/VulkanHelpers.h>

#include <assimp/DefaultLogger.hpp>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <numeric>

namespace
{
constexpr unsigned int IMPORT_FLAGS =
    aiProcess_JoinIdenticalVertices | aiProcess_Triangulate |
    aiProcess_ValidateDataStructure | aiProcess_ImproveCacheLocality |
    aiProcess_RemoveRedundantMaterials | aiProcess_FindInvalidData |
    aiProcess_GenUVCoords | aiProcess_OptimizeMeshes |
    aiProcess_OptimizeGraph | aiProcess_FlipUVs;
}

ModelManager::ModelManager()
{
	Assimp::DefaultLogger::create("AssimpLog.txt",
	                              Assimp::Logger::VERBOSE);
}

ModelManager::~ModelManager() noexcept
{
	Assimp::DefaultLogger::kill();
}

void ModelManager::SetResouces(vk::Device device,
                               vk::PhysicalDevice physicalDevice,
                               vk::CommandPool commandPool,
                               vk::Queue workQueue)
{
	m_Device         = std::move(device);
	m_PhysicalDevice = std::move(physicalDevice);
	m_CommandPool    = std::move(commandPool);
	m_WorkQueue      = std::move(workQueue);
}

void ModelManager::LoadModel(std::string modelName,
                              const std::filesystem::path& modelPath)
{
	Assimp::Importer modelImpoter{};
	const aiScene* const scene =
	    modelImpoter.ReadFile(modelPath.string(), IMPORT_FLAGS);
	if (scene == nullptr)
		throw std::runtime_error{ "Failed to load model" };

	const std::span sceneMeshes{ scene->mMeshes, scene->mNumMeshes };
	const unsigned int vertexCount = std::accumulate(
	    begin(sceneMeshes), end(sceneMeshes), 0u,
	    [](const unsigned int lhs, const aiMesh* const mesh) {
		    return lhs + mesh->mNumVertices;
	    });
	const unsigned int indexCount = std::accumulate(
	    begin(sceneMeshes), end(sceneMeshes), 0u,
	    [](const unsigned int lhs, const aiMesh* const mesh) {
		    return lhs + mesh->mNumFaces * 3;
	    });
	const vk::DeviceSize vertexBufferSize =
	    static_cast<vk::DeviceSize>(vertexCount) * sizeof(Vertex);
	const vk::DeviceSize indexBufferSize =
	    static_cast<vk::DeviceSize>(indexCount) * sizeof(unsigned int);

	const auto [vertexStagingBuffer, vertexStagingMemory] =
	    createDeviceBuffer(
	        vertexBufferSize,
	        vk::BufferUsageFlags{
	            vk::BufferUsageFlagBits::eTransferSrc },
	        vk::MemoryPropertyFlags{
	            vk::MemoryPropertyFlagBits::eHostVisible |
	            vk::MemoryPropertyFlagBits::eHostCoherent },
	        m_Device, m_PhysicalDevice);

	const auto [indexStagingBuffer, indexStagingMemory] =
	    createDeviceBuffer(
	        indexBufferSize,
	        vk::BufferUsageFlags{
	            vk::BufferUsageFlagBits::eTransferSrc },
	        vk::MemoryPropertyFlags{
	            vk::MemoryPropertyFlagBits::eHostVisible |
	            vk::MemoryPropertyFlagBits::eHostCoherent },
	        m_Device, m_PhysicalDevice);

	// Apparently the copy operations need to be done in one go always?
	std::vector<Vertex> vertices{};
	vertices.reserve(vertexCount);
	std::vector<unsigned int> indices{};
	indices.reserve(indexCount);

	for (const aiMesh* const mesh : sceneMeshes)
	{
		const unsigned int numVertices = mesh->mNumVertices;
		for (unsigned int i{ 0 }; i < numVertices; ++i)
		{
			const aiVector3D v{ mesh->mVertices[i] };
			const aiVector3D t{ mesh->mTextureCoords[0][i] };
			assert(0.f <= t.x && t.x <= 1.f);
			assert(0.f <= t.y && t.y <= 1.f);
			assert(0.f <= t.z && t.z <= 1.f);
			vertices.push_back({
			    .m_Position          = { v[0], v[1], v[2] },
			    .m_Color             = { 1.f, 1.f, 1.f },
			    .m_TextureCoordinate = { t.x, t.y },
			});
		}

		const std::span meshFaces{ mesh->mFaces, mesh->mNumFaces };
		for (const aiFace& face : meshFaces)
		{
			indices.append_range(
			    std::span{ face.mIndices, face.mNumIndices });
		}
	}

	void* const vertexPtr =
	    m_Device.mapMemory(vertexStagingMemory, vk::DeviceSize{ 0 },
	                       vertexBufferSize, vk::MemoryMapFlags{});
	void* const indexPtr =
	    m_Device.mapMemory(indexStagingMemory, vk::DeviceSize{ 0 },
	                       indexBufferSize, vk::MemoryMapFlags{});

	std::memcpy(vertexPtr, vertices.data(), vertexBufferSize);
	std::memcpy(indexPtr, indices.data(), indexBufferSize);

	m_Device.unmapMemory(indexStagingMemory);
	m_Device.unmapMemory(vertexStagingMemory);

	const auto [vertexBuffer, vertexBufferMemory] = createDeviceBuffer(
	    vertexBufferSize,
	    vk::BufferUsageFlags{ vk::BufferUsageFlagBits::eTransferDst |
	                          vk::BufferUsageFlagBits::eVertexBuffer },
	    vk::MemoryPropertyFlags{
	        vk::MemoryPropertyFlagBits::eDeviceLocal },
	    m_Device, m_PhysicalDevice);

	const auto [indexBuffer, indexBufferMemory] = createDeviceBuffer(
	    indexBufferSize,
	    vk::BufferUsageFlags{ vk::BufferUsageFlagBits::eTransferDst |
	                          vk::BufferUsageFlagBits::eIndexBuffer },
	    vk::MemoryPropertyFlags{
	        vk::MemoryPropertyFlagBits::eDeviceLocal },
	    m_Device, m_PhysicalDevice);

	copyBuffer(vertexBuffer, vertexStagingBuffer, vertexBufferSize,
	           m_CommandPool, m_Device, m_WorkQueue);
	copyBuffer(indexBuffer, indexStagingBuffer, indexBufferSize,
	           m_CommandPool, m_Device, m_WorkQueue);

	m_Device.free(indexStagingMemory);
	m_Device.destroy(indexStagingBuffer);
	m_Device.free(vertexStagingMemory);
	m_Device.destroy(vertexStagingBuffer);

	m_LoadedModels.emplace_back(
	    std::move(modelName), vertexCount, vertexBuffer,
	    vertexBufferMemory, indexCount, indexBuffer, indexBufferMemory);
}

void ModelManager::RenderAllModels(vk::CommandBuffer commandBuffer)
{
	constexpr vk::DeviceSize offset{ 0 };
	for (const Model& model : m_LoadedModels)
	{
		commandBuffer.bindVertexBuffers(0, { model.m_VertexBuffer },
		                                { offset });
		commandBuffer.bindIndexBuffer(model.m_IndexBuffer, 0,
		                              vk::IndexType::eUint32);

		commandBuffer.drawIndexed(
		    static_cast<std::uint32_t>(model.m_IndexCount), 1, 0, 0, 0);
	}
}

void ModelManager::UnloadAllModels()
{
	for (const Model& model : m_LoadedModels)
	{
		m_Device.destroy(model.m_IndexBuffer);
		m_Device.destroy(model.m_VertexBuffer);
		m_Device.free(model.m_IndexBufferMemory);
		m_Device.free(model.m_VertexBufferMemory);
	}
	m_LoadedModels.clear();
}
