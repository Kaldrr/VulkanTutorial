#include <VulkanTutorial/VulkanHelpers.h>

vk::RenderPass CreateRenderPass(const vk::Device device,
                                const VkFormat colorFormat,
                                const VkFormat depthFormat,
                                const std::uint32_t sampleCount)
{
	const std::array attachments{
		// Color attachment
		vk::AttachmentDescription{
			.format         = static_cast<vk::Format>(colorFormat),
			.samples        = static_cast<vk::SampleCountFlagBits>(sampleCount),
			.loadOp         = vk::AttachmentLoadOp::eClear,
			.storeOp        = vk::AttachmentStoreOp::eStore,
			.stencilLoadOp  = vk::AttachmentLoadOp::eDontCare,
			.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
			.initialLayout  = vk::ImageLayout::eUndefined,
			.finalLayout    = vk::ImageLayout::eColorAttachmentOptimal,
		},
		// Depth stencil attachment
		vk::AttachmentDescription{
			.format         = static_cast<vk::Format>(depthFormat),
			.samples        = static_cast<vk::SampleCountFlagBits>(sampleCount),
			.loadOp         = vk::AttachmentLoadOp::eClear,
			.storeOp        = vk::AttachmentStoreOp::eDontCare,
			.stencilLoadOp  = vk::AttachmentLoadOp::eDontCare,
			.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
			.initialLayout  = vk::ImageLayout::eUndefined,
			.finalLayout    = vk::ImageLayout::eDepthStencilAttachmentOptimal,
		},
		// MSAA attachment
		vk::AttachmentDescription{
			.format         = static_cast<vk::Format>(colorFormat),
			.samples        = vk::SampleCountFlagBits::e1,
			.loadOp         = vk::AttachmentLoadOp::eDontCare,
			.storeOp        = vk::AttachmentStoreOp::eDontCare,
			.stencilLoadOp  = vk::AttachmentLoadOp::eDontCare,
			.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
			.initialLayout  = vk::ImageLayout::eUndefined,
			.finalLayout    = vk::ImageLayout::ePresentSrcKHR,
		},
	};

	constexpr vk::AttachmentReference ColorAttachmentRef{
		.attachment = 0U,
		.layout     = vk::ImageLayout::eColorAttachmentOptimal,
	};
	constexpr vk::AttachmentReference DepthAttachmentRef{
		.attachment = 1U,
		.layout     = vk::ImageLayout::eDepthStencilAttachmentOptimal,
	};
	constexpr vk::AttachmentReference ColorAttachmentResolveRef{
		.attachment = 2U,
		.layout     = vk::ImageLayout::eColorAttachmentOptimal,
	};

	const vk::SubpassDescription subpassDescription{
		.pipelineBindPoint       = vk::PipelineBindPoint::eGraphics,
		.colorAttachmentCount    = 1U,
		.pColorAttachments       = &ColorAttachmentRef,
		.pResolveAttachments     = &ColorAttachmentResolveRef,
		.pDepthStencilAttachment = &DepthAttachmentRef,
	};

	constexpr vk::SubpassDependency Dependency{
		.srcSubpass   = VK_SUBPASS_EXTERNAL,
		.dstSubpass   = 0U,
		.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
						vk::PipelineStageFlagBits::eEarlyFragmentTests,
		.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
						vk::PipelineStageFlagBits::eEarlyFragmentTests,
		.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
						 vk::AccessFlagBits::eDepthStencilAttachmentWrite,
	};

	return device.createRenderPass(vk::RenderPassCreateInfo{
		.attachmentCount = static_cast<std::uint32_t>(size(attachments)),
		.pAttachments    = attachments.data(),
		.subpassCount    = 1U,
		.pSubpasses      = &subpassDescription,
		.dependencyCount = 1U,
		.pDependencies   = &Dependency,
	});
}

std::tuple<vk::PipelineColorBlendStateCreateInfo, vk::PipelineLayout>
CreatePipelineLayoutInfo(const vk::Device device,
						 const vk::DescriptorSetLayout descriptorSetLayout)
{
	// Band-aid as we need to return address outside of the
	// function...
	constexpr static vk::PipelineColorBlendAttachmentState ColorBlendState{
		.blendEnable         = VK_FALSE,
		.srcColorBlendFactor = vk::BlendFactor::eOne,
		.dstColorBlendFactor = vk::BlendFactor::eZero,
		.colorBlendOp        = vk::BlendOp::eAdd,
		.srcAlphaBlendFactor = vk::BlendFactor::eOne,
		.dstAlphaBlendFactor = vk::BlendFactor::eZero,
		.alphaBlendOp        = vk::BlendOp::eAdd,
		.colorWriteMask =
			vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
			vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
	};

	return {
		vk::PipelineColorBlendStateCreateInfo{
			.logicOpEnable   = VK_FALSE,
			.logicOp         = vk::LogicOp::eCopy,
			.attachmentCount = 1U,
			.pAttachments    = &ColorBlendState,
			.blendConstants  = std::array<float, 4>{ 0.F, 0.F, 0.F, 0.F },
		},
		device.createPipelineLayout(vk::PipelineLayoutCreateInfo{
			.setLayoutCount = 1U,
			.pSetLayouts    = &descriptorSetLayout,
		}),
	};
}

std::uint32_t FindMemoryType(const vk::PhysicalDevice physicalDevice,
                             const vk::MemoryPropertyFlags memoryProperties,
                             const std::uint32_t typeFilter)
{
	const vk::PhysicalDeviceMemoryProperties deviceMemoryProperties =
		physicalDevice.getMemoryProperties();

	for (std::uint32_t i{ 0U }; i < deviceMemoryProperties.memoryTypeCount; ++i)
	{
		if (static_cast<bool>(typeFilter & (1U << i)) &&
			(deviceMemoryProperties.memoryTypes.at(i).propertyFlags &
			 memoryProperties) == memoryProperties)
		{
			return i;
		}
	}
	throw std::runtime_error{ "Failed to find suitable memory for buffer" };
}

std::tuple<vk::Buffer, vk::DeviceMemory> CreateDeviceBuffer(
    const vk::DeviceSize bufferSize,
    const vk::BufferUsageFlags bufferFlags,
    const vk::MemoryPropertyFlags memoryFlags,
    const vk::Device device,
    const vk::PhysicalDevice physicalDevie)
{
	const vk::Buffer deviceBuffer = device.createBuffer(vk::BufferCreateInfo{
		.size        = bufferSize,
		.usage       = bufferFlags,
		.sharingMode = vk::SharingMode::eExclusive,
	});

	const vk::MemoryRequirements memoryRequirements =
		device.getBufferMemoryRequirements(deviceBuffer);

	const vk::DeviceMemory deviceMemory =
		device.allocateMemory(vk::MemoryAllocateInfo{
			.allocationSize  = vk::DeviceSize{ memoryRequirements.size },
			.memoryTypeIndex = FindMemoryType(physicalDevie, memoryFlags,
											  memoryRequirements.memoryTypeBits),
		});
	device.bindBufferMemory(deviceBuffer, deviceMemory, vk::DeviceSize{ 0 });

	return std::tuple{ deviceBuffer, deviceMemory };
}

void CopyBuffer(const vk::Buffer dstBuffer,
                const vk::Buffer srcBuffer,
                const vk::DeviceSize size,
                const vk::CommandPool commandPool,
                const vk::Device device,
                const vk::Queue graphicsQueue)
{
	// Create a command buffer to copy buffers around
	const vk::CommandBuffer commandBuffer =
		BeginSingleTimeCommands(device, commandPool);
	commandBuffer.copyBuffer(srcBuffer, dstBuffer,
							 vk::BufferCopy{
								 .srcOffset = vk::DeviceSize{ 0 },
								 .dstOffset = vk::DeviceSize{ 0 },
								 .size      = vk::DeviceSize{ size },
							 });
	EndSingleTimeCommands(commandBuffer, graphicsQueue);
}

vk::CommandBuffer BeginSingleTimeCommands(const vk::Device device,
										  const vk::CommandPool commandPool)
{
	const std::vector<vk::CommandBuffer> commandBuffers =
		device.allocateCommandBuffers(vk::CommandBufferAllocateInfo{
			.commandPool        = commandPool,
			.level              = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1,
		});

	assert(commandBuffers.size() == 1);
	vk::CommandBuffer commandBuffer = commandBuffers.at(0);

	commandBuffer.begin(vk::CommandBufferBeginInfo{
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
	});

	return commandBuffer;
}

void EndSingleTimeCommands(const vk::CommandBuffer commandBuffer,
						   const vk::Queue queue)
{
	commandBuffer.end();

	queue.submit(vk::ArrayProxy{
		vk::SubmitInfo{
			.commandBufferCount = 1,
			.pCommandBuffers    = &commandBuffer,
		},
	});
	queue.waitIdle();
}

void CopyBufferToImage(const vk::Buffer buffer,
                       const vk::Image image,
                       const uint32_t width,
                       const uint32_t height,
                       const vk::Device device,
                       const vk::CommandPool commandPool,
                       const vk::Queue queue)
{
	const vk::CommandBuffer commandBuffer =
		BeginSingleTimeCommands(device, commandPool);

	const vk::BufferImageCopy region{
		.bufferOffset      = vk::DeviceSize{ 0 },
		.bufferRowLength   = 0U,
		.bufferImageHeight = 0U,
		.imageSubresource =
			vk::ImageSubresourceLayers{
				.aspectMask     = vk::ImageAspectFlagBits::eColor,
				.mipLevel       = 0U,
				.baseArrayLayer = 0U,
				.layerCount     = 1U,
			},
		.imageOffset = vk::Offset3D{ .x = 0U, .y = 0U, .z = 0U },
		.imageExtent = vk::Extent3D{ .width = width, .height = height, .depth = 1U }
	};
	commandBuffer.copyBufferToImage(buffer, image,
									vk::ImageLayout::eTransferDstOptimal,
									vk::ArrayProxy{ region });

	EndSingleTimeCommands(commandBuffer, queue);
}

void TransitionImageLayout(const vk::Image image,
                           [[maybe_unused]] const vk::Format format,
                           const vk::ImageLayout oldLayout,
                           const vk::ImageLayout newLayout,
                           const vk::Device device,
                           const vk::CommandPool commandPool,
                           const vk::Queue workQueue)
{
	const vk::CommandBuffer commandBuffer =
		BeginSingleTimeCommands(device, commandPool);

	vk::ImageMemoryBarrier barrier{
		.oldLayout           = oldLayout,
		.newLayout           = newLayout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image               = image,
		.subresourceRange =
			vk::ImageSubresourceRange{
				.aspectMask     = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel   = 0U,
				.levelCount     = 1U,
				.baseArrayLayer = 0U,
				.layerCount     = 1U,
			},
	};

	vk::PipelineStageFlags sourceStage{};
	vk::PipelineStageFlags destinationStage{};
	if (oldLayout == vk::ImageLayout::eUndefined &&
		newLayout == vk::ImageLayout::eTransferDstOptimal)
	{
		barrier.srcAccessMask = vk::AccessFlags{};
		barrier.dstAccessMask =
			vk::AccessFlags{ vk::AccessFlagBits::eTransferWrite };

		sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
		destinationStage = vk::PipelineStageFlagBits::eTransfer;
	}
	else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
			 newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
	{
		barrier.srcAccessMask =
			vk::AccessFlags{ vk::AccessFlagBits::eTransferWrite };
		barrier.dstAccessMask = vk::AccessFlags{ vk::AccessFlagBits::eShaderRead };

		sourceStage      = vk::PipelineStageFlagBits::eTransfer;
		destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
	}
	else
	{
		throw std::invalid_argument("unsupported layout transition!");
	}

	commandBuffer.pipelineBarrier(
		sourceStage, destinationStage, vk::DependencyFlags{},
		vk::ArrayProxy<const vk::MemoryBarrier>{},
		vk::ArrayProxy<const vk::BufferMemoryBarrier>{},
		vk::ArrayProxy<const vk::ImageMemoryBarrier>{ barrier });

	EndSingleTimeCommands(commandBuffer, workQueue);
}
