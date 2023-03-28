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

struct Vertex
{
	QVector2D m_Position{};
	QVector3D m_Color{};

	static_assert(sizeof(QVector2D) == sizeof(std::array<float, 2>));
	static_assert(sizeof(QVector3D) == sizeof(std::array<float, 3>));

	constexpr static vk::VertexInputBindingDescription
	getBindingDescription()
	{
		constexpr vk::VertexInputBindingDescription bindingDescription{
			0u,
			sizeof(Vertex),
			vk::VertexInputRate::eVertex,
		};

		return bindingDescription;
	}

	constexpr static std::array<vk::VertexInputAttributeDescription, 2>
	getAttributeDescriptions()
	{
		constexpr std::array<vk::VertexInputAttributeDescription, 2>
		    attributeDescriptions{
			    // Position description
			    vk::VertexInputAttributeDescription{
			        0,
			        0,
			        vk::Format::eR32G32Sfloat,
			        offsetof(Vertex, m_Position),
			    },
			    // Color description
			    vk::VertexInputAttributeDescription{
			        1,
			        0,
			        vk::Format::eR32G32B32Sfloat,
			        offsetof(Vertex, m_Color),
			    },
		    };

		return attributeDescriptions;
	}
};

constexpr std::array<Vertex, 4> VertexData{
	Vertex{
	    QVector2D{ -0.5f, -0.5f },
	    QVector3D{ 1.0f, 1.0f, 1.0f },
	},
	Vertex{
	    QVector2D{ 0.5f, -0.5f },
	    QVector3D{ 0.0f, 1.0f, 0.0f },
	},
	Vertex{
	    QVector2D{ 0.5f, 0.5f },
	    QVector3D{ 1.0f, 0.0f, 0.0f },
	},
	Vertex{
	    QVector2D{ -0.5f, 0.5f },
	    QVector3D{ 0.0f, 0.0f, 1.0f },
	},
};
constexpr std::array<std::uint16_t, 6> IndexData{ 0, 1, 2, 2, 3, 0 };

[[nodiscard]] vk::RenderPass createRenderPass(
    const vk::Device& device,
    const VkFormat colorFormat,
    [[maybe_unused]] const VkFormat depthFormat,
    const std::uint32_t sampleCount)
{
	const vk::AttachmentDescription colorDescription{
		vk::AttachmentDescriptionFlags{},
		vk::Format{ colorFormat },
		static_cast<vk::SampleCountFlagBits>(sampleCount),
		vk::AttachmentLoadOp::eClear,
		vk::AttachmentStoreOp::eStore,
		vk::AttachmentLoadOp::eDontCare,
		vk::AttachmentStoreOp::eDontCare,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::ePresentSrcKHR,
	};
	constexpr vk::AttachmentReference colorAttachmentRef{
		0u,
		vk::ImageLayout::eColorAttachmentOptimal,
	};

	const vk::SubpassDescription subpassDescription{
		vk::SubpassDescriptionFlags{},
		vk::PipelineBindPoint::eGraphics,
		0,
		nullptr,
		1u,
		&colorAttachmentRef,
		nullptr,
		nullptr,
		0,
		nullptr
	};

	const vk::RenderPassCreateInfo renderPassInfo{
		vk::RenderPassCreateFlags{}, 1u, &colorDescription, 1,
		&subpassDescription,         0,  nullptr,
	};

	return device.createRenderPass(renderPassInfo);
}

[[nodiscard]] std::tuple<vk::PipelineColorBlendStateCreateInfo,
                         vk::PipelineLayout>
createPipelineLayoutInfo(
    const vk::Device& device,
    const vk::DescriptorSetLayout& descriptorSetLayout)
{
	// Band-aid as we need to return address outside of the function...
	constexpr static vk::PipelineColorBlendAttachmentState
	    colorBlendState{
		    VK_FALSE,
		    vk::BlendFactor::eOne,
		    vk::BlendFactor::eZero,
		    vk::BlendOp::eAdd,
		    vk::BlendFactor::eOne,
		    vk::BlendFactor::eZero,
		    vk::BlendOp::eAdd,
		    vk::ColorComponentFlags{ vk::ColorComponentFlagBits::eR |
		                             vk::ColorComponentFlagBits::eG |
		                             vk::ColorComponentFlagBits::eB |
		                             vk::ColorComponentFlagBits::eA },
	    };

	constexpr vk::PipelineColorBlendStateCreateInfo
	    colorBlendCreateInfo{
		    vk::PipelineColorBlendStateCreateFlags{},
		    VK_FALSE,
		    vk::LogicOp::eCopy,
		    1u,
		    &colorBlendState,
		    std::array<float, 4>{ 0.f, 0.f, 0.f, 0.f },
	    };

	const vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{
		vk::PipelineLayoutCreateFlags{},
		1u,
		&descriptorSetLayout,
		0u,
		nullptr,
	};

	return { colorBlendCreateInfo,
		     device.createPipelineLayout(pipelineLayoutCreateInfo) };
}

std::uint32_t findMemoryType(
    const vk::PhysicalDevice physicalDevice,
    const vk::MemoryPropertyFlags memoryProperties,
    const std::uint32_t typeFilter)
{
	const vk::PhysicalDeviceMemoryProperties deviceMemoryProperties =
	    physicalDevice.getMemoryProperties();

	for (std::uint32_t i{ 0u };
	     i < deviceMemoryProperties.memoryTypeCount; ++i)
	{
		if ((typeFilter & (1 << i)) &&
		    (deviceMemoryProperties.memoryTypes[i].propertyFlags &
		     memoryProperties) == memoryProperties)
		{
			return i;
		}
	}
	throw std::runtime_error{
		"Failed to find suitable memory for buffer"
	};
}

std::tuple<vk::Buffer, vk::DeviceMemory> createDeviceBuffer(
    const vk::DeviceSize bufferSize,
    const vk::BufferUsageFlags bufferFlags,
    const vk::MemoryPropertyFlags memoryFlags,
    const vk::Device& device,
    const vk::PhysicalDevice& physicalDevie)
{
	const vk::BufferCreateInfo bufferInfo{
		vk::BufferCreateFlags{},     bufferSize, bufferFlags,
		vk::SharingMode::eExclusive, 0,          nullptr,
	};

	vk::Buffer deviceBuffer = device.createBuffer(bufferInfo);

	const vk::MemoryRequirements memoryRequirements =
	    device.getBufferMemoryRequirements(deviceBuffer);

	const vk::MemoryAllocateInfo allocInfo{
		vk::DeviceSize{ memoryRequirements.size },
		findMemoryType(physicalDevie, memoryFlags,
		               memoryRequirements.memoryTypeBits),
	};
	vk::DeviceMemory deviceMemory = device.allocateMemory(allocInfo);
	device.bindBufferMemory(deviceBuffer, deviceMemory,
	                        vk::DeviceSize{ 0 });

	return std::tuple{ deviceBuffer, deviceMemory };
}

void copyBuffer(const vk::Buffer& dstBuffer,
                const vk::Buffer& srcBuffer,
                const vk::DeviceSize size,
                const vk::CommandPool& commandPool,
                const vk::Device& device,
                const vk::Queue& graphicsQueue)
{
	// Create a command buffer to copy buffers around
	const vk::CommandBufferAllocateInfo allocInfo{
		commandPool,
		vk::CommandBufferLevel::ePrimary,
		1,
		nullptr,
	};

	const std::vector<vk::CommandBuffer> commandBuffers =
	    device.allocateCommandBuffers(allocInfo);
	assert(commandBuffers.size() == 1);
	constexpr vk::CommandBufferBeginInfo beginInfo{
		vk::CommandBufferUsageFlags{
		    vk::CommandBufferUsageFlagBits::eOneTimeSubmit },
		nullptr, nullptr
	};

	const vk::CommandBuffer& commandBuffer = commandBuffers.at(0);
	commandBuffer.begin(beginInfo);
	commandBuffer.copyBuffer(srcBuffer, dstBuffer,
	                         vk::BufferCopy{ vk::DeviceSize{ 0 },
	                                         vk::DeviceSize{ 0 },
	                                         vk::DeviceSize{ size } });
	commandBuffer.end();

	const std::array<vk::SubmitInfo, 1> bufferSubmitInfo{
		vk::SubmitInfo{ 0, nullptr, nullptr, 1, &commandBuffer, 0,
		                nullptr, nullptr }
	};
	graphicsQueue.submit(bufferSubmitInfo);
	graphicsQueue.waitIdle();
}
} // namespace

VulkanRenderer::VulkanRenderer(QVulkanWindow* const window,
                               const bool msaa)
    : m_Window{ window }
{
	if (msaa)
	{
		const QList<int> counts = window->supportedSampleCounts();
		qDebug() << "Supported sample counts:" << counts;
		for (int s = 16; s >= 4; s /= 2)
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

	// No result?
	return m_Device.createShaderModule(shaderInfo);
}

void VulkanRenderer::createVertexBuffer()
{
	constexpr vk::DeviceSize memorySize{ sizeof(VertexData) };

	const vk::PhysicalDevice physicalDevice{
		m_Window->physicalDevice()
	};

	const auto [stagingBuffer, stagingMemory] = createDeviceBuffer(
	    memorySize,
	    vk::BufferUsageFlags{ vk::BufferUsageFlagBits::eTransferSrc },
	    vk::MemoryPropertyFlags{
	        vk::MemoryPropertyFlagBits::eHostVisible |
	        vk::MemoryPropertyFlagBits::eHostCoherent },
	    m_Device, physicalDevice);

	void* const devicePtr = m_Device.mapMemory(
	    stagingMemory, 0, memorySize, vk::MemoryMapFlags{});
	std::memcpy(devicePtr, static_cast<const void*>(&VertexData),
	            memorySize);
	m_Device.unmapMemory(stagingMemory);

	std::tie(m_VertexBuffer, m_VertexBufferMemory) = createDeviceBuffer(
	    memorySize,
	    vk::BufferUsageFlags{ vk::BufferUsageFlagBits::eTransferDst |
	                          vk::BufferUsageFlagBits::eVertexBuffer },
	    vk::MemoryPropertyFlags{
	        vk::MemoryPropertyFlagBits::eDeviceLocal },
	    m_Device, physicalDevice);

	copyBuffer(m_VertexBuffer, stagingBuffer, memorySize,
	           vk::CommandPool{ m_Window->graphicsCommandPool() },
	           m_Device, vk::Queue{ m_Window->graphicsQueue() });

	m_Device.destroy(stagingBuffer);
	m_Device.free(stagingMemory);
}

void VulkanRenderer::createIndexBuffer()
{
	constexpr vk::DeviceSize memorySize{ sizeof(IndexData) };

	const vk::PhysicalDevice physicalDevice{
		m_Window->physicalDevice()
	};

	const auto [stagingBuffer, stagingMemory] = createDeviceBuffer(
	    memorySize,
	    vk::BufferUsageFlags{ vk::BufferUsageFlagBits::eTransferSrc },
	    vk::MemoryPropertyFlags{
	        vk::MemoryPropertyFlagBits::eHostVisible |
	        vk::MemoryPropertyFlagBits::eHostCoherent },
	    m_Device, physicalDevice);

	void* const devicePtr = m_Device.mapMemory(
	    stagingMemory, 0, memorySize, vk::MemoryMapFlags{});
	std::memcpy(devicePtr, static_cast<const void*>(&IndexData),
	            memorySize);
	m_Device.unmapMemory(stagingMemory);

	std::tie(m_IndexBuffer, m_IndexBufferMemory) = createDeviceBuffer(
	    memorySize,
	    vk::BufferUsageFlags{ vk::BufferUsageFlagBits::eTransferDst |
	                          vk::BufferUsageFlagBits::eIndexBuffer },
	    vk::MemoryPropertyFlags{
	        vk::MemoryPropertyFlagBits::eDeviceLocal },
	    m_Device, physicalDevice);

	copyBuffer(m_IndexBuffer, stagingBuffer, memorySize,
	           vk::CommandPool{ m_Window->graphicsCommandPool() },
	           m_Device, vk::Queue{ m_Window->graphicsQueue() });

	m_Device.destroy(stagingBuffer);
	m_Device.free(stagingMemory);
}

void VulkanRenderer::createDescriptorSetLayout()
{
	constexpr vk::DescriptorSetLayoutBinding uboLayoutBinding{
		0u,
		vk::DescriptorType::eUniformBuffer,
		1u,
		vk::ShaderStageFlags{ vk::ShaderStageFlagBits::eVertex },
		nullptr,
	};

	const vk::DescriptorSetLayoutCreateInfo layoutInfo{
		vk::DescriptorSetLayoutCreateFlags{}, 1u, &uboLayoutBinding,
		nullptr
	};

	m_DescriptorSetLayout =
	    m_Device.createDescriptorSetLayout(layoutInfo);
}

void VulkanRenderer::createUniformBuffers()
{
	constexpr vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

	const std::uint32_t frameCount = static_cast<std::uint32_t>(
	    QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT);
	const vk::PhysicalDevice physicalDevice{
		m_Window->physicalDevice()
	};
	for (std::uint32_t i{ 0 }; i < frameCount; ++i)
	{
		std::tie(m_UniformBuffers[i], m_UniformDeviceMemory[i]) =
		    createDeviceBuffer(
		        bufferSize,
		        vk::BufferUsageFlags{
		            vk::BufferUsageFlagBits::eUniformBuffer },
		        vk::MemoryPropertyFlags{
		            vk::MemoryPropertyFlagBits::eHostVisible |
		            vk::MemoryPropertyFlagBits::eHostCoherent },
		        m_Device, physicalDevice);

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
	constexpr std::uint32_t concurrentFrames =
	    static_cast<std::uint32_t>(
	        QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT);

	constexpr vk::DescriptorPoolSize poolSize{
		vk::DescriptorType::eUniformBuffer, concurrentFrames
	};

	const vk::DescriptorPoolCreateInfo poolInfo{
		vk::DescriptorPoolCreateFlags{},
		concurrentFrames,
		1u,
		&poolSize,
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
		static_cast<std::uint32_t>(
		    QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT),
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

	for (int i{ 0 }; i < QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT; ++i)
	{
		const vk::DescriptorBufferInfo bufferInfo{
			m_UniformBuffers[i],
			0,
			sizeof(UniformBufferObject),
		};

		const vk::WriteDescriptorSet descriptorWrite{
			m_DescriptorSets[i],
			0u,
			0u,
			1u,
			vk::DescriptorType::eUniformBuffer,
			nullptr,
			&bufferInfo,
			nullptr,
			nullptr,
		};

		m_Device.updateDescriptorSets(
		    vk::ArrayProxy{ descriptorWrite },
		    vk::ArrayProxy<const vk::CopyDescriptorSet>{});
	}
}

void VulkanRenderer::initResources()
{
	m_Device = vk::Device{ m_Window->device() };

	createVertexBuffer();
	createIndexBuffer();

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
	constexpr std::array<vk::VertexInputAttributeDescription, 2>
	    vertexAttributeDescription = Vertex::getAttributeDescriptions();

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
		nullptr,
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
	// Window has been minimised, using this size for framebuffer is
	// illegal
	// + when window will be visible again it should return to
	// previous size or at least we will get an event about resizing
	// again :)
	if (size.height() == 0 || size.width() == 0)
		return;

	const int swapChainImageCount = m_Window->swapChainImageCount();

	fmt::print(
	    "Creating SwapChainResources for size [{}x{}] and {} images\n",
	    size.width(), size.height(), swapChainImageCount);

	for (int i{ 0 }; i < swapChainImageCount; ++i)
	{
		const vk::ImageView swapChainImage{
			m_Window->swapChainImageView(i)
		};

		const vk::FramebufferCreateInfo framebufferInfo{
			vk::FramebufferCreateFlags{},
			m_RenderPass,
			1,
			&swapChainImage,
			static_cast<std::uint32_t>(size.width()),
			static_cast<std::uint32_t>(size.height()),
			1
		};

		m_Framebuffers[i] = m_Device.createFramebuffer(framebufferInfo);
	}
}

void VulkanRenderer::releaseSwapChainResources()
{
	const int swapChainImageCount = m_Window->swapChainImageCount();
	for (int i{ 0 }; i < swapChainImageCount; ++i)
	{
		m_Device.destroy(m_Framebuffers[i]);
	}
}

void VulkanRenderer::releaseResources()
{
	m_Device.destroy(m_GraphicsPipeline);
	m_Device.destroy(m_PipelineLayout);
	m_Device.destroy(m_RenderPass);

	for (int i{ 0 }; i < QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT; ++i)
	{
		m_Device.destroy(m_UniformBuffers[i]);
		m_Device.freeMemory(m_UniformDeviceMemory[i]);
	}
	m_Device.destroy(m_DescriptorPool);
	m_Device.destroy(m_DescriptorSetLayout);

	m_Device.destroy(m_IndexBuffer);
	m_Device.free(m_IndexBufferMemory);

	m_Device.destroy(m_VertexBuffer);
	m_Device.free(m_VertexBufferMemory);
}

void VulkanRenderer::startNextFrame()
{
	const QSize size = m_Window->swapChainImageSize();
	// Window not visible, no need to render anything
	if (size.height() == 0 && size.width() == 0)
		return;

	// TODO: Difference between currentFrame and
	// currentSwapChainImageIndex???
	const int currentFrame = m_Window->currentSwapChainImageIndex();

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
		m_Framebuffers[currentFrame],
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
	commandBuffer.setViewport(0u, 1u, &viewport);
	commandBuffer.setScissor(0u, 1u, &scissor);

	constexpr vk::DeviceSize offset{ 0 };
	commandBuffer.bindVertexBuffers(0, 1, &m_VertexBuffer, &offset);
	commandBuffer.bindIndexBuffer(m_IndexBuffer, 0,
	                              vk::IndexType::eUint16);
	commandBuffer.bindDescriptorSets(
	    vk::PipelineBindPoint::eGraphics, m_PipelineLayout, 0,
	    vk::ArrayProxy{ m_DescriptorSets[currentFrame] },
	    vk::ArrayProxy<const uint32_t>{});
	commandBuffer.drawIndexed(
	    static_cast<std::uint32_t>(IndexData.size()), 1, 0, 0, 0);

	commandBuffer.endRenderPass();

	m_Window->frameReady();
	m_Window->requestUpdate();
}
