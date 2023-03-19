#include <VulkanApp/VulkanRenderer.h>

#include <QFile>
#include <QVulkanFunctions>

#include <random>

// Note that the vertex data and the projection matrix assume OpenGL.
// With Vulkan Y is negated in clip space and the near/far plane is at
// 0/1 instead of -1/1. These will be corrected for by an extra
// transformation when calculating the modelview-projection matrix.
namespace
{
constexpr std::array<float, 15> vertexData{
	// Y up, front = CCW
	1.0f, 0.5f, 1.0f, 0.0f,  0.0f, -0.5f, -0.5f, 0.0f,
	1.0f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f,  1.0f
};

constexpr std::size_t UNIFORM_DATA_SIZE = 16 * sizeof(float);

template <std::integral T>
constexpr T aligned(const T v, const T byteAlign)
{
	return (v + byteAlign - 1) & ~(byteAlign - 1);
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

VkShaderModule VulkanRenderer::createShader(const QString& name)
{
	QFile file{ name };
	if (!file.open(QIODevice::ReadOnly))
	{
		qWarning("Failed to read shader %s", qPrintable(name));
		return VK_NULL_HANDLE;
	}
	const QByteArray blob = file.readAll();
	file.close();

	const VkShaderModuleCreateInfo shaderInfo{
		.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = static_cast<std::size_t>(blob.size()),
		.pCode =
		    reinterpret_cast<const std::uint32_t*>(blob.constData())
	};
	VkShaderModule shaderModule{};
	VkResult err = m_DeviceFunctions->vkCreateShaderModule(
	    m_Window->device(), &shaderInfo, nullptr, &shaderModule);
	if (err != VK_SUCCESS)
	{
		qWarning("Failed to create shader module: %d", err);
		return VK_NULL_HANDLE;
	}

	return shaderModule;
}

void VulkanRenderer::initResources()
{
	qDebug("initResources");

	const VkDevice dev = m_Window->device();
	m_DeviceFunctions =
	    m_Window->vulkanInstance()->deviceFunctions(dev);

	// Prepare the vertex and uniform data. The vertex data will never
	// change so one buffer is sufficient regardless of the value of
	// QVulkanWindow::CONCURRENT_FRAME_COUNT. Uniform data is changing
	// per frame however so active frames have to have a dedicated copy.

	// Use just one memory allocation and one buffer. We will then
	// specify the appropriate offsets for uniform buffers in the
	// VkDescriptorBufferInfo. Have to watch out for
	// VkPhysicalDeviceLimits::minUniformBufferOffsetAlignment, though.

	// The uniform buffer is not strictly required in this example, we
	// could have used push constants as well since our single matrix
	// (64 bytes) fits into the spec mandated minimum limit of 128
	// bytes. However, once that limit is not sufficient, the per-frame
	// buffers, as shown below, will become necessary.

	const int concurrentFrameCount = m_Window->concurrentFrameCount();
	const VkPhysicalDeviceLimits* const pdevLimits =
	    &m_Window->physicalDeviceProperties()->limits;
	const VkDeviceSize uniAlign =
	    pdevLimits->minUniformBufferOffsetAlignment;
	qDebug("uniform buffer offset alignment is %u",
	       static_cast<uint>(uniAlign));

	// Our internal layout is vertex, uniform, uniform, ... with each
	// uniform buffer start offset aligned to uniAlign.
	const VkDeviceSize vertexAllocSize =
	    aligned(sizeof(vertexData), uniAlign);
	const VkDeviceSize uniformAllocSize =
	    aligned(UNIFORM_DATA_SIZE, uniAlign);

	const VkBufferCreateInfo bufInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size =
		    vertexAllocSize + concurrentFrameCount * uniformAllocSize,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
	};

	VkResult err = m_DeviceFunctions->vkCreateBuffer(
	    dev, &bufInfo, nullptr, &m_Buffer);
	if (err != VK_SUCCESS)
		qFatal("Failed to create buffer: %d", err);

	VkMemoryRequirements memReq{};
	m_DeviceFunctions->vkGetBufferMemoryRequirements(dev, m_Buffer,
	                                                 &memReq);

	const VkMemoryAllocateInfo memAllocInfo{
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, memReq.size,
		m_Window->hostVisibleMemoryIndex()
	};

	err = m_DeviceFunctions->vkAllocateMemory(dev, &memAllocInfo,
	                                          nullptr, &m_BufferMemory);
	if (err != VK_SUCCESS)
		qFatal("Failed to allocate memory: %d", err);

	err = m_DeviceFunctions->vkBindBufferMemory(dev, m_Buffer,
	                                            m_BufferMemory, 0);
	if (err != VK_SUCCESS)
		qFatal("Failed to bind buffer memory: %d", err);

	quint8* mappedMemory{ nullptr };
	err = m_DeviceFunctions->vkMapMemory(
	    dev, m_BufferMemory, 0, memReq.size, 0,
	    reinterpret_cast<void**>(&mappedMemory));
	if (err != VK_SUCCESS)
		qFatal("Failed to map memory: %d", err);
	memcpy(mappedMemory, vertexData.data(), sizeof(vertexData));

	const QMatrix4x4 ident{};
	for (int i = 0; i < concurrentFrameCount; ++i)
	{
		const VkDeviceSize offset =
		    vertexAllocSize + i * uniformAllocSize;
		memcpy(mappedMemory + offset, ident.constData(),
		       16 * sizeof(float));

		m_UniformBufferInfo[i] = { .buffer = m_Buffer,
			                       .offset = offset,
			                       .range  = uniformAllocSize };
	}
	m_DeviceFunctions->vkUnmapMemory(dev, m_BufferMemory);

	constexpr VkVertexInputBindingDescription vertexBindingDesc{
		0, // binding
		5 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX
	};
	constexpr std::array<VkVertexInputAttributeDescription, 2>
	    vertexAttrDesc{
		    // position
		    VkVertexInputAttributeDescription{ 0, // location
		                                       0, // binding
		                                       VK_FORMAT_R32G32_SFLOAT,
		                                       0 },
		    // color
		    VkVertexInputAttributeDescription{
		        1, 0, VK_FORMAT_R32G32B32_SFLOAT, 2 * sizeof(float) }
	    };

	const VkPipelineVertexInputStateCreateInfo vertexInputInfo{
		.sType =
		    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.pNext                           = nullptr,
		.flags                           = 0,
		.vertexBindingDescriptionCount   = 1,
		.pVertexBindingDescriptions      = &vertexBindingDesc,
		.vertexAttributeDescriptionCount = 2,
		.pVertexAttributeDescriptions    = vertexAttrDesc.data()
	};

	// Set up descriptor set and its layout.
	const VkDescriptorPoolSize descPoolSizes{
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		static_cast<uint32_t>(concurrentFrameCount)
	};
	const VkDescriptorPoolCreateInfo descPoolInfo{
		.sType   = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = static_cast<std::uint32_t>(concurrentFrameCount),
		.poolSizeCount = 1,
		.pPoolSizes    = &descPoolSizes
	};

	err = m_DeviceFunctions->vkCreateDescriptorPool(
	    dev, &descPoolInfo, nullptr, &m_DescriptorPool);
	if (err != VK_SUCCESS)
		qFatal("Failed to create descriptor pool: %d", err);

	constexpr VkDescriptorSetLayoutBinding layoutBinding{
		0, // binding
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
		VK_SHADER_STAGE_VERTEX_BIT, nullptr
	};
	const VkDescriptorSetLayoutCreateInfo descLayoutInfo{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
		1, &layoutBinding
	};
	err = m_DeviceFunctions->vkCreateDescriptorSetLayout(
	    dev, &descLayoutInfo, nullptr, &m_DescriptorSetLayout);
	if (err != VK_SUCCESS)
		qFatal("Failed to create descriptor set layout: %d", err);

	for (int i = 0; i < concurrentFrameCount; ++i)
	{
		const VkDescriptorSetAllocateInfo descSetAllocInfo{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
			m_DescriptorPool, 1, &m_DescriptorSetLayout
		};
		err = m_DeviceFunctions->vkAllocateDescriptorSets(
		    dev, &descSetAllocInfo, &m_DescriptorSet[i]);
		if (err != VK_SUCCESS)
			qFatal("Failed to allocate descriptor set: %d", err);

		const VkWriteDescriptorSet descWrite{
			.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet          = m_DescriptorSet[i],
			.descriptorCount = 1,
			.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo     = &m_UniformBufferInfo[i]
		};
		m_DeviceFunctions->vkUpdateDescriptorSets(dev, 1, &descWrite, 0,
		                                          nullptr);
	}

	// Pipeline cache
	constexpr VkPipelineCacheCreateInfo pipelineCacheInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO
	};
	err = m_DeviceFunctions->vkCreatePipelineCache(
	    dev, &pipelineCacheInfo, nullptr, &m_PipelineCache);
	if (err != VK_SUCCESS)
		qFatal("Failed to create pipeline cache: %d", err);

	// Pipeline layout
	const VkPipelineLayoutCreateInfo pipelineLayoutInfo{
		.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts    = &m_DescriptorSetLayout
	};
	err = m_DeviceFunctions->vkCreatePipelineLayout(
	    dev, &pipelineLayoutInfo, nullptr, &m_PipelineLayout);
	if (err != VK_SUCCESS)
		qFatal("Failed to create pipeline layout: %d", err);

	// Shaders
	const VkShaderModule vertShaderModule =
	    createShader(QStringLiteral(":/color_vert.spv"));
	const VkShaderModule fragShaderModule =
	    createShader(QStringLiteral(":/color_frag.spv"));

	// Graphics pipeline
	const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{
		VkPipelineShaderStageCreateInfo{
		    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		    nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule,
		    "main", nullptr },
		VkPipelineShaderStageCreateInfo{
		    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		    nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule,
		    "main", nullptr }
	};
	constexpr VkPipelineInputAssemblyStateCreateInfo ia{
		.sType =
		    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
	};

	// The viewport and scissor will be set dynamically via
	// vkCmdSetViewport/Scissor. This way the pipeline does not need to
	// be touched when resizing the window.
	constexpr VkPipelineViewportStateCreateInfo vp{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount  = 1
	};

	constexpr VkPipelineRasterizationStateCreateInfo rs{
		.sType =
		    VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode  = VK_CULL_MODE_NONE, // we want the back face as well
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.lineWidth = 1.0f
	};
	const VkPipelineMultisampleStateCreateInfo ms{
		.sType =
		    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		// Enable multisampling.
		.rasterizationSamples = m_Window->sampleCountFlagBits()
	};

	constexpr VkPipelineDepthStencilStateCreateInfo ds{
		.sType =
		    VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable  = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL
	};

	// no blend, write out all of rgba
	constexpr VkPipelineColorBlendAttachmentState att{ .colorWriteMask =
		                                                   0xF };
	const VkPipelineColorBlendStateCreateInfo cb{
		.sType =
		    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments    = &att
	};

	constexpr std::array<VkDynamicState, 2> dynEnable{
		VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
	};
	const VkPipelineDynamicStateCreateInfo dyn{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount =
		    static_cast<std::uint32_t>(dynEnable.size()),
		.pDynamicStates = dynEnable.data()
	};
	const VkGraphicsPipelineCreateInfo pipelineInfo{
		.sType      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages    = shaderStages.data(),
		.pVertexInputState   = &vertexInputInfo,
		.pInputAssemblyState = &ia,
		.pViewportState      = &vp,
		.pRasterizationState = &rs,
		.pMultisampleState   = &ms,
		.pDepthStencilState  = &ds,
		.pColorBlendState    = &cb,
		.pDynamicState       = &dyn,
		.layout              = m_PipelineLayout,
		.renderPass          = m_Window->defaultRenderPass()
	};
	err = m_DeviceFunctions->vkCreateGraphicsPipelines(
	    dev, m_PipelineCache, 1, &pipelineInfo, nullptr, &m_Pipeline);
	if (err != VK_SUCCESS)
		qFatal("Failed to create graphics pipeline: %d", err);

	if (vertShaderModule)
		m_DeviceFunctions->vkDestroyShaderModule(dev, vertShaderModule,
		                                         nullptr);
	if (fragShaderModule)
		m_DeviceFunctions->vkDestroyShaderModule(dev, fragShaderModule,
		                                         nullptr);
}

void VulkanRenderer::initSwapChainResources()
{
	qDebug("initSwapChainResources");

	// Projection matrix
	m_Projection = m_Window->clipCorrectionMatrix(); // adjust for
	                                                 // Vulkan-OpenGL
	                                                 // clip
	// space differences
	const QSize sz = m_Window->swapChainImageSize();
	m_Projection.perspective(45.0f, sz.width() / (float)sz.height(),
	                         0.01f, 100.0f);
	m_Projection.translate(0, 0, -4);
}

void VulkanRenderer::releaseSwapChainResources()
{
	qDebug("releaseSwapChainResources");
}

void VulkanRenderer::releaseResources()
{
	qDebug("releaseResources");

	const VkDevice dev = m_Window->device();

	if (m_Pipeline)
	{
		m_DeviceFunctions->vkDestroyPipeline(dev, m_Pipeline, nullptr);
		m_Pipeline = VK_NULL_HANDLE;
	}

	if (m_PipelineLayout)
	{
		m_DeviceFunctions->vkDestroyPipelineLayout(
		    dev, m_PipelineLayout, nullptr);
		m_PipelineLayout = VK_NULL_HANDLE;
	}

	if (m_PipelineCache)
	{
		m_DeviceFunctions->vkDestroyPipelineCache(dev, m_PipelineCache,
		                                          nullptr);
		m_PipelineCache = VK_NULL_HANDLE;
	}

	if (m_DescriptorSetLayout)
	{
		m_DeviceFunctions->vkDestroyDescriptorSetLayout(
		    dev, m_DescriptorSetLayout, nullptr);
		m_DescriptorSetLayout = VK_NULL_HANDLE;
	}

	if (m_DescriptorPool)
	{
		m_DeviceFunctions->vkDestroyDescriptorPool(
		    dev, m_DescriptorPool, nullptr);
		m_DescriptorPool = VK_NULL_HANDLE;
	}

	if (m_Buffer)
	{
		m_DeviceFunctions->vkDestroyBuffer(dev, m_Buffer, nullptr);
		m_Buffer = VK_NULL_HANDLE;
	}

	if (m_BufferMemory)
	{
		m_DeviceFunctions->vkFreeMemory(dev, m_BufferMemory, nullptr);
		m_BufferMemory = VK_NULL_HANDLE;
	}
}

void VulkanRenderer::startNextFrame()
{
	const VkDevice dev       = m_Window->device();
	const VkCommandBuffer cb = m_Window->currentCommandBuffer();
	const QSize sz           = m_Window->swapChainImageSize();

	constexpr VkClearColorValue clearColor{ { 0, 0, 0, 1 } };
	constexpr VkClearDepthStencilValue clearDS{ 1, 0 };
	constexpr std::array<VkClearValue, 3> clearValues{
		VkClearValue{ .color = clearColor },
		VkClearValue{ .depthStencil = clearDS },
		VkClearValue{ .color = clearColor },
	};

	const VkRenderPassBeginInfo rpBeginInfo{
		.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass  = m_Window->defaultRenderPass(),
		.framebuffer = m_Window->currentFramebuffer(),
		.renderArea = { .extent = { .width = static_cast<std::uint32_t>(
		                                sz.width()),
		                            .height =
		                                static_cast<std::uint32_t>(
		                                    sz.height()) } },
		.clearValueCount =
		    m_Window->sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT
		        ? 3u
		        : 2u,
		.pClearValues = clearValues.data()
	};
	const VkCommandBuffer cmdBuf = m_Window->currentCommandBuffer();
	m_DeviceFunctions->vkCmdBeginRenderPass(cmdBuf, &rpBeginInfo,
	                                        VK_SUBPASS_CONTENTS_INLINE);

	quint8* p;
	VkResult err = m_DeviceFunctions->vkMapMemory(
	    dev, m_BufferMemory,
	    m_UniformBufferInfo[m_Window->currentFrame()].offset,
	    UNIFORM_DATA_SIZE, 0, reinterpret_cast<void**>(&p));
	if (err != VK_SUCCESS)
		qFatal("Failed to map memory: %d", err);
	QMatrix4x4 m = m_Projection;

	m.rotate(m_Rotations[0], 1, 0, 0);
	m.rotate(m_Rotations[1], 0, 1, 0);
	m.rotate(m_Rotations[2], 0, 0, 1);

	memcpy(p, m.constData(), 16 * sizeof(float));
	m_DeviceFunctions->vkUnmapMemory(dev, m_BufferMemory);

	// Not exactly a real animation system, just advance on every frame
	// for now.

	static std::mt19937 rngDev{ std::random_device{}() };
	// static std::uniform_int_distribution<int> rng{0, 2};
	static std::bernoulli_distribution rng{ 0.005 };

	static int idx = 0;
	m_Rotations[idx] += 1.0f;
	if (m_Rotations[idx] >= 360.f)
	{
		m_Rotations[idx] = 0.f;
	} // m_Rotation += rng(rngDev);
	if (rng(rngDev))
	{
		++idx;
		idx = idx % m_Rotations.size();
	}

	m_DeviceFunctions->vkCmdBindPipeline(
	    cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
	m_DeviceFunctions->vkCmdBindDescriptorSets(
	    cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1,
	    &m_DescriptorSet[m_Window->currentFrame()], 0, nullptr);
	const VkDeviceSize vbOffset = 0;
	m_DeviceFunctions->vkCmdBindVertexBuffers(cb, 0, 1, &m_Buffer,
	                                          &vbOffset);

	const VkViewport viewport{ .x     = 0,
		                       .y     = 0,
		                       .width = static_cast<float>(sz.width()),
		                       .height =
		                           static_cast<float>(sz.height()),
		                       .minDepth = 0,
		                       .maxDepth = 1 };
	m_DeviceFunctions->vkCmdSetViewport(cb, 0, 1, &viewport);

	const VkRect2D scissor{
		.offset = { .x = 0, .y = 0 },
		.extent = { .width = static_cast<std::uint32_t>(viewport.width),
		            .height =
		                static_cast<std::uint32_t>(viewport.height) }
	};
	m_DeviceFunctions->vkCmdSetScissor(cb, 0, 1, &scissor);

	m_DeviceFunctions->vkCmdDraw(cb, 3, 1, 0, 0);

	m_DeviceFunctions->vkCmdEndRenderPass(cmdBuf);

	m_Window->frameReady();
	m_Window->requestUpdate(); // render continuously, throttled by the
	                           // presentation rate
}
