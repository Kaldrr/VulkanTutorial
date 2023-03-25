#include <VulkanLoadingScreen/VulkanRenderer.h>

#include <QFile>
#include <QVulkanFunctions>

#include <fmt/core.h>

#include <filesystem>
#include <numbers>

namespace
{
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

constexpr std::array<Vertex, 3> VertexData{
	Vertex{
	    QVector2D{ 0.0f, -0.5f },
	    QVector3D{ 1.0f, 0.0f, 0.0f },
	},
	Vertex{
	    QVector2D{ 0.5f, 0.5f },
	    QVector3D{ 0.0f, 1.0f, 0.0f },
	},
	Vertex{
	    QVector2D{ -0.5f, 0.5f },
	    QVector3D{ 0.0f, 0.0f, 1.0f },
	},
};

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
createPipelineLayoutInfo(const vk::Device& device)
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

	constexpr vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{
		vk::PipelineLayoutCreateFlags{}, 0u, nullptr, 0u, nullptr,
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
	constexpr vk::BufferCreateInfo bufferInfo{
		vk::BufferCreateFlags{},
		vk::DeviceSize{ sizeof(VertexData) },
		vk::BufferUsageFlagBits::eVertexBuffer,
		vk::SharingMode::eExclusive,
		0,
		nullptr,
	};

	m_VertexBuffer = m_Device.createBuffer(bufferInfo);

	const vk::MemoryRequirements memoryRequirements =
	    m_Device.getBufferMemoryRequirements(m_VertexBuffer);
	const vk::MemoryAllocateInfo allocInfo{
		vk::DeviceSize{ memoryRequirements.size },
		findMemoryType(vk::PhysicalDevice{ m_Window->physicalDevice() },
		               vk::MemoryPropertyFlags{
		                   vk::MemoryPropertyFlagBits::eHostVisible |
		                   vk::MemoryPropertyFlagBits::eHostCoherent },
		               memoryRequirements.memoryTypeBits),
	};

	m_VertexBufferMemory = m_Device.allocateMemory(allocInfo);
	m_Device.bindBufferMemory(m_VertexBuffer, m_VertexBufferMemory, 0);

	void* const devicePtr = m_Device.mapMemory(
	    m_VertexBufferMemory, 0, bufferInfo.size, vk::MemoryMapFlags{});
	std::memcpy(devicePtr, static_cast<const void*>(&VertexData),
	            sizeof(VertexData));
	m_Device.unmapMemory(m_VertexBufferMemory);
}

void VulkanRenderer::initResources()
{
	m_Device = vk::Device{ m_Window->device() };

	createVertexBuffer();

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
		    vk::FrontFace::eClockwise,
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
	vk::PipelineColorBlendStateCreateInfo colorBlendCreateInfo{};
	std::tie(colorBlendCreateInfo, m_PipelineLayout) =
	    createPipelineLayoutInfo(m_Device);

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
	// + when window will be visible again it should return to previous
	// size or at least we will get an event about resizing again :)
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
	m_Device.destroy(m_VertexBuffer);
	m_Device.free(m_VertexBufferMemory);
}

void VulkanRenderer::startNextFrame()
{
	{
		static std::array<int, 3> activeIndices{ 0, 1, 2 };
		static float angle{ 0.0f };
		constexpr float delta =
		    120.f * std::numbers::pi_v<float> / 180.0f;

		void* const devicePtr = m_Device.mapMemory(
		    m_VertexBufferMemory, 0, sizeof(VertexData),
		    vk::MemoryMapFlags{});
		auto* const deviceVertices =
		    reinterpret_cast<std::array<Vertex, 3>*>(devicePtr);

		float f{ 0.f };
		int i{ 0 };
		for (Vertex& vertex : *deviceVertices)
		{
			vertex.m_Position =
			    QVector2D{ std::cos(angle + f), std::sin(angle + f) };
			f += delta;

			int& colorIndex = activeIndices[i];
			if (vertex.m_Color[colorIndex] == 0.f)
			{
				colorIndex = (colorIndex + 1) % 3;
			}
			vertex.m_Color[colorIndex] -= 1.f / 64.f;
			vertex.m_Color[(colorIndex + 1) % 3] += 1.f / 64.f;
			++i;
		}
		m_Device.unmapMemory(m_VertexBufferMemory);

		angle += 1.f * std::numbers::pi_v<float> / 180.0f;
	}
	const QSize size = m_Window->swapChainImageSize();
	// Window not visible, no need to render anything
	if (size.height() == 0 && size.width() == 0)
		return;

	// TODO: Difference between currentFrame and
	// currentSwapChainImageIndex???
	const int currentFrame = m_Window->currentSwapChainImageIndex();

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
	commandBuffer.draw(static_cast<std::uint32_t>(VertexData.size()), 1,
	                   0, 0);

	commandBuffer.endRenderPass();

	m_Window->frameReady();
	m_Window->requestUpdate();
}
