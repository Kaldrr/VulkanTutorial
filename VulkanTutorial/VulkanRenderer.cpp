#include <VulkanTutorial/Vertex.h>
#include <VulkanTutorial/VulkanHelpers.h>
#include <VulkanTutorial/VulkanRenderer.h>

#include <QFile>
#include <QVulkanFunctions>

#include <fmt/core.h>

#include <array>
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

VulkanRenderer::VulkanRenderer(QVulkanWindow& window,
                               const bool msaa)
    : m_Window{ &window }
    , m_ConcurrentFrameCount{ static_cast<std::uint32_t>(
	      m_Window->concurrentFrameCount()) }
{
	if (msaa)
	{
		const QList<int> counts = m_Window->supportedSampleCounts();
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

	return m_Device.createShaderModule(vk::ShaderModuleCreateInfo{
		.codeSize = static_cast<std::size_t>(blob.size()),
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
		.pCode = reinterpret_cast<const std::uint32_t*>(blob.constData()),
	});
}

void VulkanRenderer::CreateDescriptorSetLayout()
{
	constexpr std::array<vk::DescriptorSetLayoutBinding, 2> DescriptorSetLayouts{
		// UniformBufferObject layout
		vk::DescriptorSetLayoutBinding{
			.binding         = 0U,
			.descriptorType  = vk::DescriptorType::eUniformBuffer,
			.descriptorCount = 1U,
			.stageFlags      = vk::ShaderStageFlagBits::eVertex,
		},
		// Texture image sampler layout
		vk::DescriptorSetLayoutBinding{
			.binding         = 1U,
			.descriptorType  = vk::DescriptorType::eCombinedImageSampler,
			.descriptorCount = 1U,
			.stageFlags      = vk::ShaderStageFlagBits::eFragment,
		},
	};
	m_DescriptorSetLayout =
		m_Device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
			.bindingCount = DescriptorSetLayouts.size(),
			.pBindings    = DescriptorSetLayouts.data(),
		});
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

	const float windowRatio = static_cast<float>(currentSize.width()) /
							  static_cast<float>(currentSize.height());
	QMatrix4x4 perspectiveMatrix{};
	perspectiveMatrix.perspective(45.F, // This MUST be in degrees, not radians
								  windowRatio, 0.1F, 10.F);
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
		vk::DescriptorPoolSize{
			.type            = vk::DescriptorType::eUniformBuffer,
			.descriptorCount = m_ConcurrentFrameCount,
		},
		// Texture image sampler size
		vk::DescriptorPoolSize{
			.type            = vk::DescriptorType::eCombinedImageSampler,
			.descriptorCount = m_ConcurrentFrameCount,
		}
	};

	m_DescriptorPool = m_Device.createDescriptorPool(
		vk::DescriptorPoolCreateInfo{ .maxSets       = m_ConcurrentFrameCount,
									  .poolSizeCount = poolSizes.size(),
									  .pPoolSizes    = poolSizes.data() });
}

void VulkanRenderer::CreateDescriptorSets()
{
	FrameArray<vk::DescriptorSetLayout> layouts{};
	layouts.fill(m_DescriptorSetLayout);

	const vk::DescriptorSetAllocateInfo allocInfo{
		.descriptorPool     = m_DescriptorPool,
		.descriptorSetCount = m_ConcurrentFrameCount,
		.pSetLayouts        = layouts.data(),
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
			.sampler     = m_TextureSampler,
			.imageView   = m_TextureImageView,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
		};

		const vk::DescriptorSet& descriptorSet = m_DescriptorSets.at(i);

		m_Device.updateDescriptorSets(
			vk::ArrayProxy<const vk::WriteDescriptorSet>{
				// UniformBufferObject write
				vk::WriteDescriptorSet{
					.dstSet          = descriptorSet,
					.dstBinding      = 0U,
					.dstArrayElement = 0U,
					.descriptorCount = 1U,
					.descriptorType  = vk::DescriptorType::eUniformBuffer,
					.pBufferInfo     = &bufferInfo,
				},
				// Texture image sampler write
				vk::WriteDescriptorSet{
					.dstSet          = descriptorSet,
					.dstBinding      = 1U,
					.dstArrayElement = 0U,
					.descriptorCount = 1U,
					.descriptorType  = vk::DescriptorType::eCombinedImageSampler,
					.pImageInfo      = &imageInfo,
				},
			},
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
	std::memcpy(memoryPtr, static_cast<const void*>(textureImage.constBits()),
	            textureSize);
	m_Device.unmapMemory(stagingBufferMemory);

	const vk::ImageCreateInfo imageCreateInfo{
		.imageType = vk::ImageType::e2D,
		.format    = vk::Format::eR8G8B8A8Srgb,
		.extent =
			vk::Extent3D{ static_cast<std::uint32_t>(textureImage.width()),
						  static_cast<std::uint32_t>(textureImage.height()), 1U },
		.mipLevels   = 1U,
		.arrayLayers = 1U,
		.samples     = vk::SampleCountFlagBits::e1,
		.tiling      = vk::ImageTiling::eOptimal,
		.usage =
			vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		.sharingMode           = vk::SharingMode::eExclusive,
		.queueFamilyIndexCount = 0U,
		.initialLayout         = vk::ImageLayout::eUndefined,
	};

	m_TextureImage = m_Device.createImage(imageCreateInfo);

	const vk::MemoryRequirements imageRequirements =
	    m_Device.getImageMemoryRequirements(m_TextureImage);
	const vk::MemoryAllocateInfo textureAllocationInfo{
		.allocationSize  = imageRequirements.size,
		.memoryTypeIndex = FindMemoryType(
		    m_PhysicalDevice,
		    vk::MemoryPropertyFlags{ vk::MemoryPropertyFlagBits::eDeviceLocal },
			imageRequirements.memoryTypeBits),
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
	m_TextureImageView = m_Device.createImageView(vk::ImageViewCreateInfo{
		.image    = m_TextureImage,
		.viewType = vk::ImageViewType::e2D,
		.format   = vk::Format::eR8G8B8A8Srgb,
		.components =
			vk::ComponentMapping{
				.r = vk::ComponentSwizzle::eIdentity,
				.g = vk::ComponentSwizzle::eIdentity,
				.b = vk::ComponentSwizzle::eIdentity,
				.a = vk::ComponentSwizzle::eIdentity,
			},
		.subresourceRange =
			vk::ImageSubresourceRange{
				.aspectMask =
					vk::ImageAspectFlags{ vk::ImageAspectFlagBits::eColor },
				.baseMipLevel   = 0U,
				.levelCount     = 1U,
				.baseArrayLayer = 0U,
				.layerCount     = 1U,
			},
	});
}

void VulkanRenderer::CreateTextureSampler()
{
	// TODO: is there a better way to do this???
	// VulkanCpp constructor just used a reinterpret cast...
	const auto deviceProperties = std::bit_cast<vk::PhysicalDeviceProperties>(
		*m_Window->physicalDeviceProperties());

	m_TextureSampler = m_Device.createSampler(vk::SamplerCreateInfo{
		.magFilter               = vk::Filter::eLinear,
		.minFilter               = vk::Filter::eLinear,
		.mipmapMode              = vk::SamplerMipmapMode::eLinear,
		.addressModeU            = vk::SamplerAddressMode::eRepeat,
		.addressModeV            = vk::SamplerAddressMode::eRepeat,
		.addressModeW            = vk::SamplerAddressMode::eRepeat,
		.mipLodBias              = 0.F,
		.anisotropyEnable        = vk::True,
		.maxAnisotropy           = deviceProperties.limits.maxSamplerAnisotropy,
		.compareEnable           = VK_FALSE,
		.compareOp               = vk::CompareOp::eAlways,
		.minLod                  = 0.F,
		.maxLod                  = 0.F,
		.borderColor             = vk::BorderColor::eIntOpaqueBlack,
		.unnormalizedCoordinates = VK_FALSE,
	});
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
			.stage  = vk::ShaderStageFlagBits::eVertex,
			.module = vertexShaderModule,
			.pName  = "main",
		},
		// Fragment shader
		vk::PipelineShaderStageCreateInfo{
			.stage  = vk::ShaderStageFlagBits::eFragment,
			.module = fragmentShaderModule,
			.pName  = "main",
		},
	};

	constexpr std::array<vk::DynamicState, 2> DynamicStates{
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor,
	};
	const vk::PipelineDynamicStateCreateInfo pipelineDynamicState{
		.flags             = vk::PipelineDynamicStateCreateFlags{},
		.dynamicStateCount = static_cast<std::uint32_t>(DynamicStates.size()),
		.pDynamicStates    = DynamicStates.data(),
	};

	constexpr vk::VertexInputBindingDescription VertexBindingDescription =
		Vertex::GetBindingDescription();
	constexpr std::array VertexAttributeDescription =
		Vertex::GetAttributeDescriptions();

	const vk::PipelineVertexInputStateCreateInfo pipelineVertexInputInfo{
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions    = &VertexBindingDescription,
		.vertexAttributeDescriptionCount =
			static_cast<std::uint32_t>(size(VertexAttributeDescription)),
		.pVertexAttributeDescriptions = VertexAttributeDescription.data(),
	};

	constexpr vk::PipelineInputAssemblyStateCreateInfo InputAssemblyInfo{
		.topology               = vk::PrimitiveTopology::eTriangleList,
		.primitiveRestartEnable = VK_FALSE,
	};
	constexpr vk::PipelineViewportStateCreateInfo DynamicViewportInfo{
		.viewportCount = 1,
		.pViewports    = nullptr,
		.scissorCount  = 1,
		.pScissors     = nullptr,
	};

	const auto sampleCount =
	    static_cast<std::uint32_t>(m_Window->sampleCountFlagBits());
	const vk::PipelineMultisampleStateCreateInfo multisampling{
		.rasterizationSamples  = static_cast<vk::SampleCountFlagBits>(sampleCount),
		.sampleShadingEnable   = VK_FALSE,
		.minSampleShading      = 1.F,
		.pSampleMask           = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable      = VK_FALSE,
	};

	constexpr vk::PipelineRasterizationStateCreateInfo RasterizationInfo{
		.depthClampEnable        = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode             = vk::PolygonMode::eFill,
		.cullMode                = vk::CullModeFlagBits::eNone,
		.frontFace               = vk::FrontFace::eCounterClockwise,
		.depthBiasEnable         = VK_FALSE,
		.depthBiasConstantFactor = 0.F,
		.depthBiasClamp          = 0.F,
		.depthBiasSlopeFactor    = 0.F,
		.lineWidth               = 1.F,
	};

	/* Skip multisampling setup, it's handled by VulkanWindow? */
	m_RenderPass = CreateRenderPass(m_Device, m_Window->colorFormat(),
	                                m_Window->depthStencilFormat(), sampleCount);

	CreateDescriptorSetLayout();
	CreateUniformBuffers();
	CreateDescriptorPool();
	CreateDescriptorSets();

	constexpr vk::PipelineDepthStencilStateCreateInfo DepthStencil{
		.depthTestEnable       = vk::True,
		.depthWriteEnable      = vk::True,
		.depthCompareOp        = vk::CompareOp::eLess,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable     = VK_FALSE,
		.minDepthBounds        = 0.F,
		.maxDepthBounds        = 1.F,
	};

	vk::PipelineColorBlendStateCreateInfo colorBlendCreateInfo{};
	std::tie(colorBlendCreateInfo, m_PipelineLayout) =
		CreatePipelineLayoutInfo(m_Device, m_DescriptorSetLayout);

	auto [createPipelineResult, pipeline] = m_Device.createGraphicsPipeline(
		vk::PipelineCache{},
		vk::GraphicsPipelineCreateInfo{
			.stageCount          = static_cast<std::uint32_t>(shaderInfo.size()),
			.pStages             = shaderInfo.data(),
			.pVertexInputState   = &pipelineVertexInputInfo,
			.pInputAssemblyState = &InputAssemblyInfo,
			.pViewportState      = &DynamicViewportInfo,
			.pRasterizationState = &RasterizationInfo,
			.pMultisampleState   = &multisampling,
			.pDepthStencilState  = &DepthStencil,
			.pColorBlendState    = &colorBlendCreateInfo,
			.pDynamicState       = &pipelineDynamicState,
			.layout              = m_PipelineLayout,
			.renderPass          = m_RenderPass,
			.subpass             = 0,
			.basePipelineIndex   = -1,
		});

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

		m_Framebuffers.at(static_cast<std::size_t>(i)) =
			m_Device.createFramebuffer(vk::FramebufferCreateInfo{
				.renderPass      = m_RenderPass,
				.attachmentCount = attachmentImageViews.size(),
				.pAttachments    = attachmentImageViews.data(),
				.width           = static_cast<std::uint32_t>(size.width()),
				.height          = static_cast<std::uint32_t>(size.height()),
				.layers          = 1,
			});
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
		.color =
			vk::ClearColorValue{
				std::array<float, 4>{ 0.0F, 0.0F, 0.0F, 1.0F },
			},
	};
	constexpr vk::ClearValue ClearDepthStencil{
		.depthStencil =
			vk::ClearDepthStencilValue{
				.depth   = 1.F,
				.stencil = 0U,
			},
	};
	constexpr std::array<vk::ClearValue, 3> ClearValues{
		ClearColor,
		ClearDepthStencil,
		ClearColor,
	};

	const vk::RenderPassBeginInfo renderPassInfo{
		.renderPass  = m_RenderPass,
		.framebuffer = m_Framebuffers.at(static_cast<std::size_t>(currentImageIdx)),
		.renderArea =
			vk::Rect2D{
				vk::Offset2D{ 0, 0 },
				vk::Extent2D{
					static_cast<std::uint32_t>(size.width()),
					static_cast<std::uint32_t>(size.height()),
				},
			},
		.clearValueCount = sampleCount > vk::SampleCountFlagBits::e1 ? 3U : 2U,
		.pClearValues    = ClearValues.data(),
	};

	const vk::CommandBuffer commandBuffer{ m_Window->currentCommandBuffer() };
	commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
	                           m_GraphicsPipeline);

	const vk::Viewport viewport{
		.x        = 0.F,
		.y        = 0.F,
		.width    = static_cast<float>(size.width()),
		.height   = static_cast<float>(size.height()),
		.minDepth = 0.F,
		.maxDepth = 1.F,
	};
	const vk::Rect2D scissor{
		.offset = vk::Offset2D{ 0, 0 },
		.extent =
			vk::Extent2D{
				static_cast<std::uint32_t>(size.width()),
				static_cast<std::uint32_t>(size.height()),
			},
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
