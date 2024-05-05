#include <VulkanTutorial/ModelManager.h>
#include <VulkanTutorial/Vertex.h>
#include <VulkanTutorial/VulkanHelpers.h>

#include <assimp/DefaultLogger.hpp>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <execution>
#include <numeric>
#include <ranges>
#include <type_traits>

namespace
{
constexpr std::uint32_t ImportFlags =
    aiProcess_JoinIdenticalVertices | aiProcess_Triangulate |
    aiProcess_ValidateDataStructure | aiProcess_ImproveCacheLocality |
    aiProcess_RemoveRedundantMaterials | aiProcess_FindInvalidData |
    aiProcess_GenUVCoords | aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph |
    aiProcess_FlipUVs;

template <typename... Functors>
// NOLINTNEXTLINE(fuchsia-multiple-inheritance)
struct [[nodiscard]] Overload : Functors...
{
    using Functors::operator()...;
};

template <typename... Functors>
Overload(Functors...) -> Overload<Functors...>;
} // namespace

ModelManager::ModelManager()
{
	Assimp::DefaultLogger::create("AssimpLog.txt", Assimp::Logger::VERBOSE);
}

ModelManager::~ModelManager() noexcept
{
	UnloadAllModels();
	Assimp::DefaultLogger::kill();
}

void ModelManager::SetResouces(const vk::Device device,
                               const vk::PhysicalDevice physicalDevice,
                               const vk::CommandPool commandPool,
                               const vk::Queue workQueue)
{
	static_assert(std::is_trivially_copy_assignable_v<vk::Device>);
	static_assert(std::is_trivially_copy_assignable_v<vk::PhysicalDevice>);
	static_assert(std::is_trivially_copy_assignable_v<vk::CommandPool>);
	static_assert(std::is_trivially_copy_assignable_v<vk::Queue>);

	m_Device         = device;
	m_PhysicalDevice = physicalDevice;
	m_CommandPool    = commandPool;
	m_WorkQueue      = workQueue;
}

void ModelManager::LoadModel(const std::string_view modelName,
							 const std::filesystem::path& modelPath)
{
	Assimp::Importer modelImpoter{};
	const aiScene* const scene =
		modelImpoter.ReadFile(modelPath.string(), ImportFlags);
	if (scene == nullptr)
	{
		throw std::runtime_error{ "Failed to load model" };
	}

	const std::span sceneMeshes{ scene->mMeshes, scene->mNumMeshes };

	// Reduce with different types is difficult D:

	const std::uint32_t vertexCount = std::reduce(
		std::execution::par_unseq, begin(sceneMeshes), end(sceneMeshes), 0U,
		Overload{
			[](const aiMesh* const lhs, const std::uint32_t rhs) {
				return lhs->mNumVertices + rhs;
			},
			[](const std::uint32_t lhs, const aiMesh* const rhs) {
				return lhs + rhs->mNumVertices;
			},
			[](const std::uint32_t lhs, const std::uint32_t rhs) {
				return lhs + rhs;
			},
			[](const aiMesh* const lhs, const aiMesh* const rhs) {
				return lhs->mNumVertices + rhs->mNumVertices;
			},
		});
	const std::uint32_t indexCount = std::reduce(
		std::execution::par_unseq, begin(sceneMeshes), end(sceneMeshes), 0U,
		Overload{
			[](const aiMesh* const lhs, const std::uint32_t rhs) {
				return lhs->mNumFaces * 3 + rhs;
			},
			[](const std::uint32_t lhs, const aiMesh* const rhs) {
				return lhs + rhs->mNumFaces * 3;
			},
			[](const std::uint32_t lhs, const std::uint32_t rhs) {
				return lhs + rhs;
			},
			[](const aiMesh* const lhs, const aiMesh* const rhs) {
				return lhs->mNumFaces * 3 + rhs->mNumFaces * 3;
			},
		});

	const vk::DeviceSize vertexBufferSize =
		static_cast<vk::DeviceSize>(vertexCount) * sizeof(Vertex);
	const vk::DeviceSize indexBufferSize =
		static_cast<vk::DeviceSize>(indexCount) * sizeof(std::uint32_t);

	const auto [vertexStagingBuffer, vertexStagingMemory] = CreateDeviceBuffer(
		vertexBufferSize,
		vk::BufferUsageFlags{ vk::BufferUsageFlagBits::eTransferSrc },
		vk::MemoryPropertyFlags{ vk::MemoryPropertyFlagBits::eHostVisible |
								 vk::MemoryPropertyFlagBits::eHostCoherent },
		m_Device, m_PhysicalDevice);

	const auto [indexStagingBuffer, indexStagingMemory] = CreateDeviceBuffer(
		indexBufferSize,
		vk::BufferUsageFlags{ vk::BufferUsageFlagBits::eTransferSrc },
		vk::MemoryPropertyFlags{ vk::MemoryPropertyFlagBits::eHostVisible |
								 vk::MemoryPropertyFlagBits::eHostCoherent },
		m_Device, m_PhysicalDevice);

	// Apparently the copy operations need to be done in one go
	// always?
	std::vector<Vertex> vertices{};
	vertices.reserve(vertexCount);
	std::vector<unsigned int> indices{};
	indices.reserve(indexCount);

	for (const aiMesh* const mesh : sceneMeshes)
	{
		const std::span<const aiVector3D> meshVertices{ mesh->mVertices,
												  mesh->mNumVertices };
		const std::span<const aiVector3D> meshTextureCoords{ mesh->mTextureCoords[0],
													   mesh->mNumVertices };

		constexpr auto TransformFn = [](const auto& zipElement) {
			const auto& [vertex, textureCoord] = zipElement;
			assert(0.F <= textureCoord.x && textureCoord.x <= 1.F);
			assert(0.F <= textureCoord.y && textureCoord.y <= 1.F);
			assert(0.F <= textureCoord.z && textureCoord.z <= 1.F);
			return Vertex{
				.Position          = { vertex[0], vertex[1], vertex[2] },
				.Color             = { 1.F, 1.F, 1.F },
				.TextureCoordinate = { textureCoord.x, textureCoord.y },
			};
		};
		std::ranges::transform(
			std::ranges::views::zip(meshVertices, meshTextureCoords),
			std::back_inserter(vertices), TransformFn);

		const std::span meshFaces{ mesh->mFaces, mesh->mNumFaces };
		for (const aiFace& face : meshFaces)
		{
			std::ranges::copy(std::span{ face.mIndices, face.mNumIndices },
							  std::back_inserter(indices));
		}
	}

	void* const vertexPtr =
		m_Device.mapMemory(vertexStagingMemory, vk::DeviceSize{ 0 },
						   vertexBufferSize, vk::MemoryMapFlags{});
	void* const indexPtr =
		m_Device.mapMemory(indexStagingMemory, vk::DeviceSize{ 0 }, indexBufferSize,
						   vk::MemoryMapFlags{});

	std::memcpy(vertexPtr, vertices.data(), vertexBufferSize);
	std::memcpy(indexPtr, indices.data(), indexBufferSize);

	m_Device.unmapMemory(indexStagingMemory);
	m_Device.unmapMemory(vertexStagingMemory);

	const auto [vertexBuffer, vertexBufferMemory] = CreateDeviceBuffer(
		vertexBufferSize,
		vk::BufferUsageFlags{ vk::BufferUsageFlagBits::eTransferDst |
							  vk::BufferUsageFlagBits::eVertexBuffer },
		vk::MemoryPropertyFlags{ vk::MemoryPropertyFlagBits::eDeviceLocal },
		m_Device, m_PhysicalDevice);

	const auto [indexBuffer, indexBufferMemory] = CreateDeviceBuffer(
		indexBufferSize,
		vk::BufferUsageFlags{ vk::BufferUsageFlagBits::eTransferDst |
							  vk::BufferUsageFlagBits::eIndexBuffer },
		vk::MemoryPropertyFlags{ vk::MemoryPropertyFlagBits::eDeviceLocal },
		m_Device, m_PhysicalDevice);

	CopyBuffer(vertexBuffer, vertexStagingBuffer, vertexBufferSize, m_CommandPool,
			   m_Device, m_WorkQueue);
	CopyBuffer(indexBuffer, indexStagingBuffer, indexBufferSize, m_CommandPool,
			   m_Device, m_WorkQueue);

	m_Device.free(indexStagingMemory);
	m_Device.destroy(indexStagingBuffer);
	m_Device.free(vertexStagingMemory);
	m_Device.destroy(vertexStagingBuffer);

	m_LoadedModels.emplace_back(std::string{ modelName }, vertexCount, vertexBuffer,
								vertexBufferMemory, indexCount, indexBuffer,
								indexBufferMemory);
}

void ModelManager::RenderAllModels(vk::CommandBuffer commandBuffer) const
{
	constexpr vk::DeviceSize Offset{ 0 };
	for (const Model& model : m_LoadedModels)
	{
		commandBuffer.bindVertexBuffers(0, { model.VertexBuffer }, { Offset });
		commandBuffer.bindIndexBuffer(model.IndexBuffer, 0, vk::IndexType::eUint32);

		commandBuffer.drawIndexed(model.IndexCount, 1, 0, 0, 0);
	}
}

void ModelManager::UnloadAllModels()
{
	for (const Model& model : m_LoadedModels)
	{
		m_Device.destroy(model.IndexBuffer);
		m_Device.destroy(model.VertexBuffer);
		m_Device.free(model.IndexBufferMemory);
		m_Device.free(model.VertexBufferMemory);
	}
	m_LoadedModels.clear();
}
