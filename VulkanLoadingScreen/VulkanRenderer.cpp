#include <VulkanLoadingScreen/Vertex.h>
#include <VulkanLoadingScreen/VulkanHelpers.h>
#include <VulkanLoadingScreen/VulkanRenderer.h>

#include <QFile>
#include <QVulkanFunctions>

#include <fmt/core.h>

#include <filesystem>
#include <numbers>

// QMatrix4x4 includes a 'flag' which would make copying harder

using MatrixF4 = QGenericMatrix<4, 4, float>;

namespace
{
struct UniformBufferObject
{
	MatrixF4 m_Model{};
	MatrixF4 m_View{};
	MatrixF4 m_Perspective{};

	static_assert(sizeof(MatrixF4) == sizeof(std::array<float, 16>));
};
} // namespace

VulkanRenderer::VulkanRenderer(QVulkanWindow* const window,
                               const bool msaa)
    : m_Window{ window }
    , m_ConcurrentFrameCount{ static_cast<std::uint32_t>(
	      m_Window->concurrentFrameCount()) }
{
	if (msaa)
	{
		const QList<int> counts = window->supportedSampleCounts();
		qDebug() << "Supported sample counts:" << counts;
		for (int s{ 16 }; s >= 4; s /= 2)
		{
			if (counts.contains(s))
			{
				qDebug("Requesting sample count %d", s);
				m_Window->setSampleCount(s);
				break;
			}
		}
	}
}

[[nodiscard]] vk::ShaderModule VulkanRenderer::createShader(
    const QString& name)
{
	QFile file{ name };
	if (!file.open(QIODevice::OpenModeFlag::ReadOnly))
	{
		const std::filesystem::path pwd{
			std::filesystem::current_path()
		};
		throw std::runtime_error{ fmt::format(
			"Failed to open {}/{} shader file", pwd.string(),
			name.toStdString()) };
	}
	const QByteArray blob = file.readAll();
	file.close();

	const vk::ShaderModuleCreateInfo shaderInfo{
		vk::ShaderModuleCreateFlags{},
		static_cast<std::size_t>(blob.size()),
		reinterpret_cast<const std::uint32_t*>(blob.constData()),
	};

	return m_Device.createShaderModule(shaderInfo);
}

void VulkanRenderer::createDescriptorSetLayout()
{
	constexpr std::array<vk::DescriptorSetLayoutBinding, 2>
	    descriptorSetLayouts{
		    // UniformBufferObject layout
		    vk::DescriptorSetLayoutBinding{
		        0u,
		        vk::DescriptorType::eUniformBuffer,
		        1u,
		        vk::ShaderStageFlags{
		            vk::ShaderStageFlagBits::eVertex },
		        nullptr,
		    },
		    // Texture image sampler layout
		    vk::DescriptorSetLayoutBinding{
		        1u, vk::DescriptorType::eCombinedImageSampler, 1u,
		        vk::ShaderStageFlags{
		            vk::ShaderStageFlagBits::eFragment },
		        nullptr }
	    };
	const vk::DescriptorSetLayoutCreateInfo layoutInfo{
		vk::DescriptorSetLayoutCreateFlags{},
		vk::ArrayProxyNoTemporaries<
		    const vk::DescriptorSetLayoutBinding>{
		    descriptorSetLayouts },
		nullptr,
	};

	m_DescriptorSetLayout =
	    m_Device.createDescriptorSetLayout(layoutInfo);
}

void VulkanRenderer::createUniformBuffers()
{
	constexpr vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

	for (std::uint32_t i{ 0 }; i < m_ConcurrentFrameCount; ++i)
	{
		std::tie(m_UniformBuffers[i], m_UniformDeviceMemory[i]) =
		    createDeviceBuffer(
		        bufferSize,
		        vk::BufferUsageFlags{
		            vk::BufferUsageFlagBits::eUniformBuffer },
		        vk::MemoryPropertyFlags{
		            vk::MemoryPropertyFlagBits::eHostVisible |
		            vk::MemoryPropertyFlagBits::eHostCoherent },
		        m_Device, m_PhysicalDevice);

		// Persistent mapping, we won't be unmapping this
		m_UniformBuffersMappedMemory[i] = m_Device.mapMemory(
		    m_UniformDeviceMemory[i], vk::DeviceSize{ 0 }, bufferSize,
		    vk::MemoryMapFlags{});
	}
}

void VulkanRenderer::updateUniformBuffer(const int idx,
                                         const QSize currentSize)
{
	using Clock = std::chrono::steady_clock;
	using FloatDuration =
	    std::chrono::duration<float, std::chrono::seconds::period>;

	static Clock::time_point startTime = Clock::now();

	const Clock::time_point currentTime = Clock::now();
	const float time =
	    FloatDuration{ currentTime - startTime }.count() * 36.f;

	QMatrix4x4 modelMatrix{};
	modelMatrix.rotate(time * qDegreesToRadians(90.f),
	                   QVector3D{ 0.f, 0.f, 1.f });

	QMatrix4x4 viewMatrix{};
	viewMatrix.lookAt(QVector3D{ 2.f, 2.f, 2.f },
	                  QVector3D{ 0.f, 0.f, 0.f },
	                  QVector3D{ 0.f, 0.f, 1.f });

	QMatrix4x4 perspectiveMatrix{};
	perspectiveMatrix.perspective(
	    45.f, // This MUST be in degrees, not radians
	    static_cast<float>(currentSize.width()) /
	        static_cast<float>(currentSize.height()),
	    0.1f, 10.f);
	perspectiveMatrix(1, 1) *= -1.f;

	const UniformBufferObject ubo{
		// GenericMatrix has same layout as a plain array
		modelMatrix.toGenericMatrix<4, 4>(),
		viewMatrix.toGenericMatrix<4, 4>(),
		perspectiveMatrix.toGenericMatrix<4, 4>()
	};
	std::memcpy(m_UniformBuffersMappedMemory[idx], &ubo,
	            sizeof(UniformBufferObject));
}

void VulkanRenderer::createDescriptorPool()
{
	const std::array<vk::DescriptorPoolSize, 2> poolSizes{
		// Uniform buffer size
		vk::DescriptorPoolSize{ vk::DescriptorType::eUniformBuffer,
		                        m_ConcurrentFrameCount },
		// Texture image sampler size
		vk::DescriptorPoolSize{
		    vk::DescriptorType::eCombinedImageSampler,
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

void VulkanRenderer::createDescriptorSets()
{
	FrameArray<vk::DescriptorSetLayout> layouts{};
	layouts.fill(m_DescriptorSetLayout);

	const vk::DescriptorSetAllocateInfo allocInfo{
		m_DescriptorPool,
		m_ConcurrentFrameCount,
		layouts.data(),
		nullptr,
	};

	const vk::Result result = m_Device.allocateDescriptorSets(
	    &allocInfo, m_DescriptorSets.data());
	if (result != vk::Result::eSuccess)
	{
		throw std::runtime_error{
			"Failed to allocate descriptor sets"
		};
	}

	for (std::uint32_t i{ 0u }; i < m_ConcurrentFrameCount; ++i)
	{
		const vk::DescriptorBufferInfo bufferInfo{
			m_UniformBuffers[i], 0, sizeof(UniformBufferObject)
		};

		const vk::DescriptorImageInfo imageInfo{
			m_TextureSampler, m_TextureImageView,
			vk::ImageLayout::eShaderReadOnlyOptimal
		};

		const std::array<vk::WriteDescriptorSet, 2> descriptorWrites{
			// UniformBufferObject write
			vk::WriteDescriptorSet{ m_DescriptorSets[i], 0u, 0u, 1u,
			                        vk::DescriptorType::eUniformBuffer,
			                        nullptr, &bufferInfo, nullptr,
			                        nullptr },
			// Texture image sampler write
			vk::WriteDescriptorSet{
			    m_DescriptorSets[i], 1u, 0u, 1u,
			    vk::DescriptorType::eCombinedImageSampler, &imageInfo,
			    nullptr, nullptr },
		};

		m_Device.updateDescriptorSets(
		    vk::ArrayProxy<const vk::WriteDescriptorSet>{
		        descriptorWrites },
		    vk::ArrayProxy<const vk::CopyDescriptorSet>{});
	}
}

void VulkanRenderer::loadTextures()
{
	// TODO: extract into helper function
	QImage textureImage{ "./Textures/VikingRoom.png" };
	assert(textureImage.isNull() == false);
	textureImage.convertTo(QImage::Format::Format_RGBA8888);

	const vk::DeviceSize textureSize =
	    static_cast<vk::DeviceSize>(textureImage.sizeInBytes());

	const auto [stagingBuffer, stagingBufferMemory] =
	    createDeviceBuffer(
	        textureSize,
	        vk::BufferUsageFlags{
	            vk::BufferUsageFlagBits::eTransferSrc },
	        vk::MemoryPropertyFlags{
	            vk::MemoryPropertyFlagBits::eHostVisible |
	            vk::MemoryPropertyFlagBits::eHostCoherent },
	        m_Device, m_PhysicalDevice);

	void* const memoryPtr =
	    m_Device.mapMemory(stagingBufferMemory, vk::DeviceSize{ 0 },
	                       textureSize, vk::MemoryMapFlags{});
	std::memcpy(memoryPtr,
	            reinterpret_cast<const void*>(textureImage.constBits()),
	            textureSize);
	m_Device.unmapMemory(stagingBufferMemory);

	const vk::ImageCreateInfo imageCreateInfo{
		vk::ImageCreateFlags{},
		vk::ImageType::e2D,
		vk::Format::eR8G8B8A8Srgb,
		vk::Extent3D{ static_cast<std::uint32_t>(textureImage.width()),
		              static_cast<std::uint32_t>(textureImage.height()),
		              1u },
		1u,
		1u,
		vk::SampleCountFlagBits::e1,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlags{ vk::ImageUsageFlagBits::eTransferDst |
		                     vk::ImageUsageFlagBits::eSampled },
		vk::SharingMode::eExclusive,
		0u,
		nullptr,
		vk::ImageLayout::eUndefined,
		nullptr
	};

	m_TextureImage = m_Device.createImage(imageCreateInfo);

	const vk::MemoryRequirements imageRequirements =
	    m_Device.getImageMemoryRequirements(m_TextureImage);
	const vk::MemoryAllocateInfo textureAllocationInfo{
		imageRequirements.size,
		findMemoryType(m_PhysicalDevice,
		               vk::MemoryPropertyFlags{
		                   vk::MemoryPropertyFlagBits::eDeviceLocal },
		               imageRequirements.memoryTypeBits),
		nullptr
	};
	m_TextureImageMemory =
	    m_Device.allocateMemory(textureAllocationInfo);
	m_Device.bindImageMemory(m_TextureImage, m_TextureImageMemory,
	                         vk::DeviceSize{ 0 });

	const vk::CommandPool commandPool{
		m_Window->graphicsCommandPool()
	};
	const vk::Queue queue{ m_Window->graphicsQueue() };

	transitionImageLayout(m_TextureImage, vk::Format::eR8G8B8A8Srgb,
	                      vk::ImageLayout::eUndefined,
	                      vk::ImageLayout::eTransferDstOptimal,
	                      m_Device, commandPool, queue);
	copyBufferToImage(stagingBuffer, m_TextureImage,
	                  static_cast<std::uint32_t>(textureImage.width()),
	                  static_cast<std::uint32_t>(textureImage.height()),
	                  m_Device, commandPool, queue);
	transitionImageLayout(m_TextureImage, vk::Format::eR8G8B8A8Srgb,
	                      vk::ImageLayout::eTransferDstOptimal,
	                      vk::ImageLayout::eShaderReadOnlyOptimal,
	                      m_Device, commandPool, queue);

	m_Device.destroy(stagingBuffer);
	m_Device.free(stagingBufferMemory);
}

void VulkanRenderer::createTextureImageView()
{
	const vk::ImageViewCreateInfo viewInfo{
		vk::ImageViewCreateFlags{},
		m_TextureImage,
		vk::ImageViewType::e2D,
		vk::Format::eR8G8B8A8Srgb,
		vk::ComponentMapping{ vk::ComponentSwizzle::eIdentity,
		                      vk::ComponentSwizzle::eIdentity,
		                      vk::ComponentSwizzle::eIdentity,
		                      vk::ComponentSwizzle::eIdentity },
		vk::ImageSubresourceRange{
		    vk::ImageAspectFlags{ vk::ImageAspectFlagBits::eColor }, 0u,
		    1u, 0u, 1u },
		nullptr
	};

	m_TextureImageView = m_Device.createImageView(viewInfo);
}

void VulkanRenderer::createTextureSampler()
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
		0.f,
		VK_TRUE,
		deviceProperties.limits.maxSamplerAnisotropy,
		VK_FALSE,
		vk::CompareOp::eAlways,
		0.f,
		0.f,
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

	loadTextures();
	createTextureImageView();
	createTextureSampler();

	// Shaders
	const vk::ShaderModule vertexShaderModule =
	    createShader(QStringLiteral("./Shaders/vert.spv"));
	const vk::ShaderModule fragmentShaderModule =
	    createShader(QStringLiteral("./Shaders/frag.spv"));

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

	constexpr std::array<vk::DynamicState, 2> dynamicStates{
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor,
	};
	const vk::PipelineDynamicStateCreateInfo pipelineDynamicState{
		vk::PipelineDynamicStateCreateFlags{},
		static_cast<std::uint32_t>(dynamicStates.size()),
		dynamicStates.data(),
	};

	constexpr vk::VertexInputBindingDescription
	    vertexBindingDescription = Vertex::getBindingDescription();
	const std::span vertexAttributeDescription =
	    Vertex::getAttributeDescriptions();

	const vk::PipelineVertexInputStateCreateInfo
	    pipelineVertexInputInfo{
		    vk::PipelineVertexInputStateCreateFlags{},
		    1,
		    &vertexBindingDescription,
		    static_cast<std::uint32_t>(
		        size(vertexAttributeDescription)),
		    vertexAttributeDescription.data(),
	    };

	constexpr vk::PipelineInputAssemblyStateCreateInfo
	    inputAssemblyInfo{
		    vk::PipelineInputAssemblyStateCreateFlags{},
		    vk::PrimitiveTopology::eTriangleList,
		    VK_FALSE,
	    };
	constexpr vk::PipelineViewportStateCreateInfo dynamicViewportInfo{
		vk::PipelineViewportStateCreateFlags{}, 1, nullptr, 1, nullptr,
	};
	const std::uint32_t sampleCount =
	    static_cast<std::uint32_t>(m_Window->sampleCountFlagBits());
	const vk::PipelineMultisampleStateCreateInfo multisampling{
		vk::PipelineMultisampleStateCreateFlags{},
		static_cast<vk::SampleCountFlagBits>(sampleCount),
		VK_FALSE,
		1.f,
		nullptr,
		VK_FALSE,
		VK_FALSE,
	};

	constexpr vk::PipelineRasterizationStateCreateInfo
	    rasterizationInfo{
		    vk::PipelineRasterizationStateCreateFlags{},
		    VK_FALSE,
		    VK_FALSE,
		    vk::PolygonMode::eFill,
		    vk::CullModeFlags{ vk::CullModeFlagBits::eBack },
		    vk::FrontFace::eCounterClockwise,
		    VK_FALSE,
		    0.f,
		    0.f,
		    0.f,
		    1.f,
	    };

	/* Skip multisampling setup, it's handled by VulkanWindow? */
	m_RenderPass =
	    createRenderPass(m_Device, m_Window->colorFormat(),
	                     m_Window->depthStencilFormat(), sampleCount);

	createDescriptorSetLayout();
	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();

	constexpr vk::PipelineDepthStencilStateCreateInfo depthStencil{
		vk::PipelineDepthStencilStateCreateFlags{},
		VK_TRUE,
		VK_TRUE,
		vk::CompareOp::eLess,
		VK_FALSE,
		VK_FALSE,
		vk::StencilOpState{},
		vk::StencilOpState{},
		0.f,
		1.f,
		nullptr
	};

	vk::PipelineColorBlendStateCreateInfo colorBlendCreateInfo{};
	std::tie(colorBlendCreateInfo, m_PipelineLayout) =
	    createPipelineLayoutInfo(m_Device, m_DescriptorSetLayout);

	const vk::GraphicsPipelineCreateInfo pipelineCreateInfo{
		vk::PipelineCreateFlags{},
		static_cast<std::uint32_t>(shaderInfo.size()),
		shaderInfo.data(),
		&pipelineVertexInputInfo,
		&inputAssemblyInfo,
		nullptr,
		&dynamicViewportInfo,
		&rasterizationInfo,
		&multisampling,
		&depthStencil,
		&colorBlendCreateInfo,
		&pipelineDynamicState,
		m_PipelineLayout,
		m_RenderPass,
		0,
		VK_NULL_HANDLE,
		-1,
	};

	if (auto [res, pipeline] =
	        m_Device.createGraphicsPipeline({}, pipelineCreateInfo);
	    res != vk::Result::eSuccess)
	{
		throw std::runtime_error{ fmt::format(
			"Failed to create graphics pipeline: {}",
			vk::to_string(res)) };
	}
	else
		m_GraphicsPipeline = std::move(pipeline);

	m_Device.destroy(vertexShaderModule);
	m_Device.destroy(fragmentShaderModule);
}

void VulkanRenderer::initSwapChainResources()
{
	const QSize size = m_Window->swapChainImageSize();

	m_SwapChainImageCount =
	    static_cast<std::uint32_t>(m_Window->swapChainImageCount());
	// Window has been minimised, using this size for framebuffer is
	// illegal
	// + when window will be visible again it should return to
	// previous size or at least we will get an event about resizing
	// again :)
	if (size.height() < 5 || size.width() < 5)
		return;

	fmt::print(
	    "Creating SwapChainResources for size [{}x{}] and {} images\n",
	    size.width(), size.height(), m_SwapChainImageCount);

	const vk::ImageView depthImageView{
		m_Window->depthStencilImageView()
	};
	for (std::uint32_t i{ 0u }; i < m_SwapChainImageCount; ++i)
	{
		const std::array<vk::ImageView, 2> attachmentImageViews{
			m_Window->swapChainImageView(i), depthImageView
		};

		const vk::FramebufferCreateInfo framebufferInfo{
			vk::FramebufferCreateFlags{},
			m_RenderPass,
			attachmentImageViews,
			static_cast<std::uint32_t>(size.width()),
			static_cast<std::uint32_t>(size.height()),
			1
		};

		m_Framebuffers[i] = m_Device.createFramebuffer(framebufferInfo);
	}
}

void VulkanRenderer::releaseSwapChainResources()
{
	// TODO: Probably shouldn't be destroyed if window is small...
	for (std::uint32_t i{ 0u }; i < m_SwapChainImageCount; ++i)
		m_Device.destroy(m_Framebuffers[i]);
}

void VulkanRenderer::releaseResources()
{
	m_Device.destroy(m_GraphicsPipeline);
	m_Device.destroy(m_PipelineLayout);
	m_Device.destroy(m_RenderPass);

	for (std::uint32_t i{ 0u }; i < m_ConcurrentFrameCount; ++i)
	{
		m_Device.destroy(m_UniformBuffers[i]);
		m_Device.freeMemory(m_UniformDeviceMemory[i]);
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
	if (size.height() < 5 || size.width() < 5)
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

	updateUniformBuffer(currentFrame, size);

	const vk::SampleCountFlagBits sampleCount =
	    static_cast<vk::SampleCountFlagBits>(
	        m_Window->sampleCountFlagBits());

	constexpr vk::ClearValue clearColor{
		vk::ClearColorValue{
		    std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f },
		},
	};
	constexpr vk::ClearDepthStencilValue clearDepthStencil{ 1.f, 0u };
	constexpr std::array<vk::ClearValue, 3> clearValues{
		vk::ClearValue{ clearColor },
		vk::ClearValue{ clearDepthStencil },
		vk::ClearValue{ clearColor },
	};

	const vk::RenderPassBeginInfo renderPassInfo{
		m_RenderPass,
		m_Framebuffers[currentImageIdx],
		vk::Rect2D{
		    vk::Offset2D{ 0, 0 },
		    vk::Extent2D{ static_cast<std::uint32_t>(size.width()),
		                  static_cast<std::uint32_t>(size.height()) },
		},
		sampleCount > vk::SampleCountFlagBits::e1 ? 3u : 2u,
		clearValues.data(),
	};

	const vk::CommandBuffer commandBuffer{
		m_Window->currentCommandBuffer()
	};
	commandBuffer.beginRenderPass(renderPassInfo,
	                              vk::SubpassContents::eInline);
	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
	                           m_GraphicsPipeline);

	const vk::Viewport viewport{
		0.f,
		0.f,
		static_cast<float>(size.width()),
		static_cast<float>(size.height()),
		0.f,
		1.f,
	};
	const vk::Rect2D scissor{
		vk::Offset2D{ 0, 0 },
		vk::Extent2D{ static_cast<std::uint32_t>(size.width()),
		              static_cast<std::uint32_t>(size.height()) },
	};
	commandBuffer.setViewport(0u, vk::ArrayProxy{ viewport });
	commandBuffer.setScissor(0u, vk::ArrayProxy{ scissor });

	constexpr vk::DeviceSize offset{ 0 };

	commandBuffer.bindDescriptorSets(
	    vk::PipelineBindPoint::eGraphics, m_PipelineLayout, 0,
	    vk::ArrayProxy{ m_DescriptorSets[currentFrame] },
	    vk::ArrayProxy<const uint32_t>{});
	m_ModelManager.RenderAllModels(commandBuffer);

	commandBuffer.endRenderPass();

	m_Window->frameReady();
	m_Window->requestUpdate();
}
