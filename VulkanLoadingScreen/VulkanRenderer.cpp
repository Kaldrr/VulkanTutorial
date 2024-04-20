#include <VulkanLoadingScreen/Vertex.h>
#include <VulkanLoadingScreen/VulkanHelpers.h>
#include <VulkanLoadingScreen/VulkanRenderer.h>

#include <QFile>
#include <QVulkanFunctions>

#include <fmt/core.h>

#include <filesystem>

// QMatrix4x4 includes a 'flag' which would make copying harder

using MatrixF4 = QGenericMatrix<4, 4, float>;

namespace
{
constexpr int MinimumWindowSize = 5;

struct UniformBufferObject
{
	MatrixF4 Model;
	MatrixF4 View;
	MatrixF4 Perspective;

	constexpr static std::size_t NumberOfElements = 16;
	static_assert(sizeof(MatrixF4) == sizeof(std::array<float, NumberOfElements>));
};
} // namespace

VulkanRenderer::VulkanRenderer(QVulkanWindow* const window, const bool msaa)
    : m_Window{ window }
    , m_ConcurrentFrameCount{ static_cast<std::uint32_t>(
	      m_Window->concurrentFrameCount()) }
{
	if (msaa)
	{
		const QList<int> counts = window->supportedSampleCounts();
		qDebug() << "Supported sample counts:" << counts;

		constexpr int StartingSampleCount = 16;
		for (int samples{ StartingSampleCount }; samples >= 4; samples /= 2)
		{
			if (counts.contains(samples))
			{
				qDebug() << "Requesting sample count:" << samples;
				m_Window->setSampleCount(samples);
				break;
			}
		}
	}
}

vk::ShaderModule VulkanRenderer::CreateShader(const QString& name) const
{
	QFile file{ name };
	if (!file.open(QIODevice::OpenModeFlag::ReadOnly))
	{
		const std::filesystem::path pwd{ std::filesystem::current_path() };
		throw std::runtime_error{ fmt::format("Failed to open {}/{} shader file",
			                                  pwd.string(), name.toStdString()) };
	}
	const QByteArray blob = file.readAll();
	file.close();

	const vk::ShaderModuleCreateInfo shaderInfo{
		vk::ShaderModuleCreateFlags{},
		static_cast<std::size_t>(blob.size()),
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
		reinterpret_cast<const std::uint32_t*>(blob.constData()),
	};

	return m_Device.createShaderModule(shaderInfo);
}

void VulkanRenderer::CreateDescriptorSetLayout()
{
	constexpr std::array<vk::DescriptorSetLayoutBinding, 2> DescriptorSetLayouts{
		// UniformBufferObject layout
		vk::DescriptorSetLayoutBinding{
		    0U,
		    vk::DescriptorType::eUniformBuffer,
		    1U,
		    vk::ShaderStageFlags{ vk::ShaderStageFlagBits::eVertex },
		    nullptr,
		},
		// Texture image sampler layout
		vk::DescriptorSetLayoutBinding{
		    1U, vk::DescriptorType::eCombinedImageSampler, 1U,
		    vk::ShaderStageFlags{ vk::ShaderStageFlagBits::eFragment }, nullptr }
	};
	const vk::DescriptorSetLayoutCreateInfo layoutInfo{
		vk::DescriptorSetLayoutCreateFlags{},
		vk::ArrayProxyNoTemporaries<const vk::DescriptorSetLayoutBinding>{
		    DescriptorSetLayouts },
		nullptr,
	};

	m_DescriptorSetLayout = m_Device.createDescriptorSetLayout(layoutInfo);
}

void VulkanRenderer::CreateUniformBuffers()
{
	constexpr vk::DeviceSize BufferSize = sizeof(UniformBufferObject);

	for (std::uint32_t i{ 0 }; i < m_ConcurrentFrameCount; ++i)
	{
		vk::DeviceMemory& deviceMemory = m_UniformDeviceMemory.at(i);
		std::tie(m_UniformBuffers.at(i), deviceMemory) = CreateDeviceBuffer(
			BufferSize,
			vk::BufferUsageFlags{ vk::BufferUsageFlagBits::eUniformBuffer },
			vk::MemoryPropertyFlags{ vk::MemoryPropertyFlagBits::eHostVisible |
									 vk::MemoryPropertyFlagBits::eHostCoherent },
			m_Device, m_PhysicalDevice);

		// Persistent mapping, we won't be unmapping this
		m_UniformBuffersMappedMemory.at(i) = m_Device.mapMemory(
			deviceMemory, vk::DeviceSize{ 0 }, BufferSize, vk::MemoryMapFlags{});
	}
}

void VulkanRenderer::UpdateUniformBuffer(const int idx, const QSize currentSize)
{
	using Clock = std::chrono::steady_clock;
	using FloatDuration =
	    std::chrono::duration<float, std::chrono::seconds::period>;

	const static Clock::time_point StartTime = Clock::now();

	const Clock::time_point currentTime = Clock::now();
	const float time = FloatDuration{ currentTime - StartTime }.count() * 36.F;

	QMatrix4x4 modelMatrix{};
	constexpr float RotationSpeedDegrees = 90.F;
	constexpr float RotationSpeed        = qDegreesToRadians(RotationSpeedDegrees);
	modelMatrix.rotate(time * RotationSpeed, QVector3D{ 0.F, 0.F, 1.F });

	// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
	QMatrix4x4 viewMatrix{};
	viewMatrix.lookAt(QVector3D{ 2.F, 2.F, 2.F }, QVector3D{ 0.F, 0.F, 0.F },
	                  QVector3D{ 0.F, 0.F, 1.F });

	QMatrix4x4 perspectiveMatrix{};
	perspectiveMatrix.perspective(45.F, // This MUST be in degrees, not radians
	                              static_cast<float>(currentSize.width()) /
	                                  static_cast<float>(currentSize.height()),
	                              0.1F, 10.F);
	perspectiveMatrix(1, 1) *= -1.F;
	// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

	const UniformBufferObject ubo{ // GenericMatrix has same layout as a plain array
		                           modelMatrix.toGenericMatrix<4, 4>(),
		                           viewMatrix.toGenericMatrix<4, 4>(),
		                           perspectiveMatrix.toGenericMatrix<4, 4>()
	};
	std::memcpy(m_UniformBuffersMappedMemory.at(static_cast<std::size_t>(idx)),
				&ubo, sizeof(UniformBufferObject));
}

void VulkanRenderer::CreateDescriptorPool()
{
	const std::array<vk::DescriptorPoolSize, 2> poolSizes{
		// Uniform buffer size
		vk::DescriptorPoolSize{ vk::DescriptorType::eUniformBuffer,
		                        m_ConcurrentFrameCount },
		// Texture image sampler size
		vk::DescriptorPoolSize{ vk::DescriptorType::eCombinedImageSampler,
		                        m_ConcurrentFrameCount }
	};
	const vk::DescriptorPoolCreateInfo poolInfo{
		vk::DescriptorPoolCreateFlags{},
		m_ConcurrentFrameCount,
		poolSizes,
		nullptr,
	};

	m_DescriptorPool = m_Device.createDescriptorPool(poolInfo);
}

void VulkanRenderer::CreateDescriptorSets()
{
	FrameArray<vk::DescriptorSetLayout> layouts{};
	layouts.fill(m_DescriptorSetLayout);

	const vk::DescriptorSetAllocateInfo allocInfo{
		m_DescriptorPool,
		m_ConcurrentFrameCount,
		layouts.data(),
		nullptr,
	};

	const vk::Result result =
	    m_Device.allocateDescriptorSets(&allocInfo, m_DescriptorSets.data());
	if (result != vk::Result::eSuccess)
	{
		throw std::runtime_error{ "Failed to allocate descriptor sets" };
	}

	for (std::uint32_t i{ 0U }; i < m_ConcurrentFrameCount; ++i)
	{
		const vk::DescriptorBufferInfo bufferInfo{ m_UniformBuffers.at(i), 0,
			                                       sizeof(UniformBufferObject) };

		const vk::DescriptorImageInfo imageInfo{
			m_TextureSampler, m_TextureImageView,
			vk::ImageLayout::eShaderReadOnlyOptimal
		};

		const vk::DescriptorSet& descriptorSet = m_DescriptorSets.at(i);
		const std::array<vk::WriteDescriptorSet, 2> descriptorWrites{
			// UniformBufferObject write
			vk::WriteDescriptorSet{ descriptorSet, 0U, 0U, 1U,
			                        vk::DescriptorType::eUniformBuffer, nullptr,
			                        &bufferInfo, nullptr, nullptr },
			// Texture image sampler write
			vk::WriteDescriptorSet{ descriptorSet, 1U, 0U, 1U,
			                        vk::DescriptorType::eCombinedImageSampler,
			                        &imageInfo, nullptr, nullptr },
		};

		m_Device.updateDescriptorSets(
		    vk::ArrayProxy<const vk::WriteDescriptorSet>{ descriptorWrites },
		    vk::ArrayProxy<const vk::CopyDescriptorSet>{});
	}
}

void VulkanRenderer::LoadTextures()
{
	// TODO: extract into helper function
	QImage textureImage{ "./Textures/VikingRoom.png" };
	assert(textureImage.isNull() == false);
	textureImage.convertTo(QImage::Format::Format_RGBA8888);

	const auto textureSize =
	    static_cast<vk::DeviceSize>(textureImage.sizeInBytes());

	const auto [stagingBuffer, stagingBufferMemory] = CreateDeviceBuffer(
	    textureSize, vk::BufferUsageFlags{ vk::BufferUsageFlagBits::eTransferSrc },
	    vk::MemoryPropertyFlags{ vk::MemoryPropertyFlagBits::eHostVisible |
	                             vk::MemoryPropertyFlagBits::eHostCoherent },
	    m_Device, m_PhysicalDevice);

	void* const memoryPtr =
	    m_Device.mapMemory(stagingBufferMemory, vk::DeviceSize{ 0 }, textureSize,
	                       vk::MemoryMapFlags{});
	std::memcpy(memoryPtr,
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
				reinterpret_cast<const void*>(textureImage.constBits()),
	            textureSize);
	m_Device.unmapMemory(stagingBufferMemory);

	const vk::ImageCreateInfo imageCreateInfo{
		vk::ImageCreateFlags{},
		vk::ImageType::e2D,
		vk::Format::eR8G8B8A8Srgb,
		vk::Extent3D{ static_cast<std::uint32_t>(textureImage.width()),
		              static_cast<std::uint32_t>(textureImage.height()), 1U },
		1U,
		1U,
		vk::SampleCountFlagBits::e1,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlags{ vk::ImageUsageFlagBits::eTransferDst |
		                     vk::ImageUsageFlagBits::eSampled },
		vk::SharingMode::eExclusive,
		0U,
		nullptr,
		vk::ImageLayout::eUndefined,
		nullptr
	};

	m_TextureImage = m_Device.createImage(imageCreateInfo);

	const vk::MemoryRequirements imageRequirements =
	    m_Device.getImageMemoryRequirements(m_TextureImage);
	const vk::MemoryAllocateInfo textureAllocationInfo{
		imageRequirements.size,
		FindMemoryType(
		    m_PhysicalDevice,
		    vk::MemoryPropertyFlags{ vk::MemoryPropertyFlagBits::eDeviceLocal },
		    imageRequirements.memoryTypeBits),
		nullptr
	};
	m_TextureImageMemory = m_Device.allocateMemory(textureAllocationInfo);
	m_Device.bindImageMemory(m_TextureImage, m_TextureImageMemory,
	                         vk::DeviceSize{ 0 });

	const vk::CommandPool commandPool{ m_Window->graphicsCommandPool() };
	const vk::Queue queue{ m_Window->graphicsQueue() };

	TransitionImageLayout(
	    m_TextureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eUndefined,
	    vk::ImageLayout::eTransferDstOptimal, m_Device, commandPool, queue);
	CopyBufferToImage(stagingBuffer, m_TextureImage,
	                  static_cast<std::uint32_t>(textureImage.width()),
	                  static_cast<std::uint32_t>(textureImage.height()), m_Device,
	                  commandPool, queue);
	TransitionImageLayout(m_TextureImage, vk::Format::eR8G8B8A8Srgb,
	                      vk::ImageLayout::eTransferDstOptimal,
	                      vk::ImageLayout::eShaderReadOnlyOptimal, m_Device,
	                      commandPool, queue);

	m_Device.destroy(stagingBuffer);
	m_Device.free(stagingBufferMemory);
}

void VulkanRenderer::CreateTextureImageView()
{
	const vk::ImageViewCreateInfo viewInfo{
		vk::ImageViewCreateFlags{},
		m_TextureImage,
		vk::ImageViewType::e2D,
		vk::Format::eR8G8B8A8Srgb,
		vk::ComponentMapping{
		    vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
		    vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity },
		vk::ImageSubresourceRange{
		    vk::ImageAspectFlags{ vk::ImageAspectFlagBits::eColor }, 0U, 1U, 0U,
		    1U },
		nullptr
	};

	m_TextureImageView = m_Device.createImageView(viewInfo);
}

void VulkanRenderer::CreateTextureSampler()
{
	const vk::PhysicalDeviceProperties deviceProperties{
		*m_Window->physicalDeviceProperties()
	};

	const vk::SamplerCreateInfo samplerInfo{
		vk::SamplerCreateFlags{},
		vk::Filter::eLinear,
		vk::Filter::eLinear,
		vk::SamplerMipmapMode::eLinear,
		vk::SamplerAddressMode::eRepeat,
		vk::SamplerAddressMode::eRepeat,
		vk::SamplerAddressMode::eRepeat,
		0.F,
		VK_TRUE,
		deviceProperties.limits.maxSamplerAnisotropy,
		VK_FALSE,
		vk::CompareOp::eAlways,
		0.F,
		0.F,
		vk::BorderColor::eIntOpaqueBlack,
		VK_FALSE,
		nullptr
	};

	m_TextureSampler = m_Device.createSampler(samplerInfo);
}

void VulkanRenderer::initResources()
{
	m_Device         = vk::Device{ m_Window->device() };
	m_PhysicalDevice = vk::PhysicalDevice{ m_Window->physicalDevice() };
	VULKAN_HPP_DEFAULT_DISPATCHER.init(m_Device);

	m_ModelManager.SetResouces(m_Device, m_PhysicalDevice,
	                           m_Window->graphicsCommandPool(),
	                           m_Window->graphicsQueue());
	m_ModelManager.LoadModel("VikingRoom", "./Models/VikingRoom.obj");

	LoadTextures();
	CreateTextureImageView();
	CreateTextureSampler();

	// Shaders
	const vk::ShaderModule vertexShaderModule =
		CreateShader(QStringLiteral("./Shaders/vert.spv"));
	const vk::ShaderModule fragmentShaderModule =
		CreateShader(QStringLiteral("./Shaders/frag.spv"));

	const std::array<vk::PipelineShaderStageCreateInfo, 2> shaderInfo{
		// Vertex shader
		vk::PipelineShaderStageCreateInfo{
		    vk::PipelineShaderStageCreateFlags{},
		    vk::ShaderStageFlagBits::eVertex,
		    vertexShaderModule,
		    "main",
		    nullptr,
		},
		// Fragment shader
		vk::PipelineShaderStageCreateInfo{
		    vk::PipelineShaderStageCreateFlags{},
		    vk::ShaderStageFlagBits::eFragment,
		    fragmentShaderModule,
		    "main",
		    nullptr,
		},
	};

	constexpr std::array<vk::DynamicState, 2> DynamicStates{
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor,
	};
	const vk::PipelineDynamicStateCreateInfo pipelineDynamicState{
		vk::PipelineDynamicStateCreateFlags{},
		static_cast<std::uint32_t>(DynamicStates.size()),
		DynamicStates.data(),
	};

	constexpr vk::VertexInputBindingDescription VertexBindingDescription =
		Vertex::GetBindingDescription();
	const std::span vertexAttributeDescription = Vertex::GetAttributeDescriptions();

	const vk::PipelineVertexInputStateCreateInfo pipelineVertexInputInfo{
		vk::PipelineVertexInputStateCreateFlags{},
		1,
		&VertexBindingDescription,
		static_cast<std::uint32_t>(size(vertexAttributeDescription)),
		vertexAttributeDescription.data(),
	};

	constexpr vk::PipelineInputAssemblyStateCreateInfo InputAssemblyInfo{
		vk::PipelineInputAssemblyStateCreateFlags{},
		vk::PrimitiveTopology::eTriangleList,
		VK_FALSE,
	};
	constexpr vk::PipelineViewportStateCreateInfo DynamicViewportInfo{
		vk::PipelineViewportStateCreateFlags{}, 1, nullptr, 1, nullptr,
	};
	const auto sampleCount =
	    static_cast<std::uint32_t>(m_Window->sampleCountFlagBits());
	const vk::PipelineMultisampleStateCreateInfo multisampling{
		vk::PipelineMultisampleStateCreateFlags{},
		static_cast<vk::SampleCountFlagBits>(sampleCount),
		VK_FALSE,
		1.F,
		nullptr,
		VK_FALSE,
		VK_FALSE,
	};

	constexpr vk::PipelineRasterizationStateCreateInfo RasterizationInfo{
		vk::PipelineRasterizationStateCreateFlags{},
		VK_FALSE,
		VK_FALSE,
		vk::PolygonMode::eFill,
		vk::CullModeFlags{ vk::CullModeFlagBits::eNone },
		vk::FrontFace::eCounterClockwise,
		VK_FALSE,
		0.F,
		0.F,
		0.F,
		1.F,
	};

	/* Skip multisampling setup, it's handled by VulkanWindow? */
	m_RenderPass = CreateRenderPass(m_Device, m_Window->colorFormat(),
	                                m_Window->depthStencilFormat(), sampleCount);

	CreateDescriptorSetLayout();
	CreateUniformBuffers();
	CreateDescriptorPool();
	CreateDescriptorSets();

	constexpr vk::PipelineDepthStencilStateCreateInfo DepthStencil{
		vk::PipelineDepthStencilStateCreateFlags{},
		VK_TRUE,
		VK_TRUE,
		vk::CompareOp::eLess,
		VK_FALSE,
		VK_FALSE,
		vk::StencilOpState{},
		vk::StencilOpState{},
		0.F,
		1.F,
		nullptr
	};

	vk::PipelineColorBlendStateCreateInfo colorBlendCreateInfo{};
	std::tie(colorBlendCreateInfo, m_PipelineLayout) =
		CreatePipelineLayoutInfo(m_Device, m_DescriptorSetLayout);

	const vk::GraphicsPipelineCreateInfo pipelineCreateInfo{
		vk::PipelineCreateFlags{},
		static_cast<std::uint32_t>(shaderInfo.size()),
		shaderInfo.data(),
		&pipelineVertexInputInfo,
		&InputAssemblyInfo,
		nullptr,
		&DynamicViewportInfo,
		&RasterizationInfo,
		&multisampling,
		&DepthStencil,
		&colorBlendCreateInfo,
		&pipelineDynamicState,
		m_PipelineLayout,
		m_RenderPass,
		0,
		VK_NULL_HANDLE,
		-1,
	};

	auto [createPipelineResult, pipeline] =
		m_Device.createGraphicsPipeline(vk::PipelineCache{}, pipelineCreateInfo);
	if (createPipelineResult != vk::Result::eSuccess)
	{
		throw std::runtime_error{ fmt::format(
			"Failed to create graphics pipeline: {}",
			vk::to_string(createPipelineResult)) };
	}

	m_GraphicsPipeline = pipeline;

	m_Device.destroy(vertexShaderModule);
	m_Device.destroy(fragmentShaderModule);
}

void VulkanRenderer::initSwapChainResources()
{
	const QSize size = m_Window->swapChainImageSize();

	m_SwapChainImageCount = m_Window->swapChainImageCount();
	// Window has been minimised, using this size for framebuffer is
	// illegal
	// + when window will be visible again it should return to
	// previous size or at least we will get an event about resizing
	// again
	if (size.height() < MinimumWindowSize || size.width() < MinimumWindowSize)
	{
		return;
	}

	fmt::print("Creating SwapChainResources for size [{}x{}] and {} images\n",
	           size.width(), size.height(), m_SwapChainImageCount);

	const vk::ImageView depthImageView{ m_Window->depthStencilImageView() };
	for (int i{ 0U }; i < m_SwapChainImageCount; ++i)
	{
		const vk::ImageView msaaColorImageView{ m_Window->msaaColorImageView(i) };
		const std::array<vk::ImageView, 3> attachmentImageViews{
			msaaColorImageView, depthImageView, m_Window->swapChainImageView(i)
		};

		const vk::FramebufferCreateInfo framebufferInfo{
			vk::FramebufferCreateFlags{},
			m_RenderPass,
			attachmentImageViews,
			static_cast<std::uint32_t>(size.width()),
			static_cast<std::uint32_t>(size.height()),
			1
		};

		m_Framebuffers.at(static_cast<std::size_t>(i)) =
			m_Device.createFramebuffer(framebufferInfo);
	}
}

void VulkanRenderer::releaseSwapChainResources()
{
	// TODO: Probably shouldn't be destroyed if window is small...
	for (int i{ 0U }; i < m_SwapChainImageCount; ++i)
	{
		m_Device.destroy(m_Framebuffers.at(static_cast<std::size_t>(i)));
	}
}

void VulkanRenderer::releaseResources()
{
	m_Device.destroy(m_GraphicsPipeline);
	m_Device.destroy(m_PipelineLayout);
	m_Device.destroy(m_RenderPass);

	for (std::uint32_t i{ 0U }; i < m_ConcurrentFrameCount; ++i)
	{
		m_Device.destroy(m_UniformBuffers.at(i));
		m_Device.freeMemory(m_UniformDeviceMemory.at(i));
	}
	m_Device.destroy(m_DescriptorPool);
	m_Device.destroy(m_DescriptorSetLayout);

	m_Device.destroy(m_TextureSampler);
	m_Device.destroy(m_TextureImageView);
	m_Device.destroy(m_TextureImage);
	m_Device.free(m_TextureImageMemory);

	m_ModelManager.UnloadAllModels();

	m_PhysicalDevice = vk::PhysicalDevice{};
	m_Device         = vk::Device{};
}

void VulkanRenderer::startNextFrame()
{
	const QSize size = m_Window->swapChainImageSize();
	// Window not visible, no need to render anything
	if (size.height() < MinimumWindowSize || size.width() < MinimumWindowSize)
	{
		// Tell window we're ready, otherwise we will hang here...
		m_Window->frameReady();
		m_Window->requestUpdate();
		return;
	}
	// CurrentFrame for buffers
	const int currentFrame = m_Window->currentFrame();
	// CurrentImageIdx for everything else
	const int currentImageIdx = m_Window->currentSwapChainImageIndex();

	UpdateUniformBuffer(currentFrame, size);

	const auto sampleCount =
	    static_cast<vk::SampleCountFlagBits>(m_Window->sampleCountFlagBits());

	constexpr vk::ClearValue ClearColor{
		vk::ClearColorValue{
		    std::array<float, 4>{ 0.0F, 0.0F, 0.0F, 1.0F },
		},
	};
	constexpr vk::ClearDepthStencilValue ClearDepthStencil{ 1.F, 0U };
	constexpr std::array<vk::ClearValue, 3> ClearValues{
		vk::ClearValue{ ClearColor },
		vk::ClearValue{ ClearDepthStencil },
		vk::ClearValue{ ClearColor },
	};

	const vk::RenderPassBeginInfo renderPassInfo{
		m_RenderPass,
		m_Framebuffers.at(static_cast<std::size_t>(currentImageIdx)),
		vk::Rect2D{
		    vk::Offset2D{ 0, 0 },
		    vk::Extent2D{ static_cast<std::uint32_t>(size.width()),
		                  static_cast<std::uint32_t>(size.height()) },
		},
		sampleCount > vk::SampleCountFlagBits::e1 ? 3U : 2U,
		ClearValues.data(),
	};

	const vk::CommandBuffer commandBuffer{ m_Window->currentCommandBuffer() };
	commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
	                           m_GraphicsPipeline);

	const vk::Viewport viewport{
		0.F,
		0.F,
		static_cast<float>(size.width()),
		static_cast<float>(size.height()),
		0.F,
		1.F,
	};
	const vk::Rect2D scissor{
		vk::Offset2D{ 0, 0 },
		vk::Extent2D{ static_cast<std::uint32_t>(size.width()),
		              static_cast<std::uint32_t>(size.height()) },
	};
	commandBuffer.setViewport(0U, vk::ArrayProxy{ viewport });
	commandBuffer.setScissor(0U, vk::ArrayProxy{ scissor });

	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
									 m_PipelineLayout, 0,
									 vk::ArrayProxy{ m_DescriptorSets.at(
										 static_cast<std::size_t>(currentFrame)) },
									 vk::ArrayProxy<const uint32_t>{});
	m_ModelManager.RenderAllModels(commandBuffer);

	commandBuffer.endRenderPass();

	m_Window->frameReady();
	m_Window->requestUpdate();
}
