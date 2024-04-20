#include <VulkanLoadingScreen/VulkanHelpers.h>

vk::RenderPass CreateRenderPass(const vk::Device device,
                                const VkFormat colorFormat,
                                const VkFormat depthFormat,
                                const std::uint32_t sampleCount)
{
	const std::array attachments{
		// Color attachment
		vk::AttachmentDescription{
		    vk::AttachmentDescriptionFlags{},
			static_cast<vk::Format>(colorFormat),
		    static_cast<vk::SampleCountFlagBits>(sampleCount),
		    vk::AttachmentLoadOp::eClear,
		    vk::AttachmentStoreOp::eStore,
		    vk::AttachmentLoadOp::eDontCare,
		    vk::AttachmentStoreOp::eDontCare,
		    vk::ImageLayout::eUndefined,
		    vk::ImageLayout::eColorAttachmentOptimal,
		},
		// Depth stencil attachment
		vk::AttachmentDescription{
		    vk::AttachmentDescriptionFlags{},
			static_cast<vk::Format>(depthFormat),
		    static_cast<vk::SampleCountFlagBits>(sampleCount),
		    vk::AttachmentLoadOp::eClear,
		    vk::AttachmentStoreOp::eDontCare,
		    vk::AttachmentLoadOp::eDontCare,
		    vk::AttachmentStoreOp::eDontCare,
		    vk::ImageLayout::eUndefined,
		    vk::ImageLayout::eDepthStencilAttachmentOptimal,
		},
		// MSAA attachment
		vk::AttachmentDescription{
		    vk::AttachmentDescriptionFlags{},
			static_cast<vk::Format>(colorFormat),
		    vk::SampleCountFlagBits::e1,
		    vk::AttachmentLoadOp::eDontCare,
		    vk::AttachmentStoreOp::eDontCare,
		    vk::AttachmentLoadOp::eDontCare,
		    vk::AttachmentStoreOp::eDontCare,
		    vk::ImageLayout::eUndefined,
		    vk::ImageLayout::ePresentSrcKHR,
		},
	};

	constexpr vk::AttachmentReference ColorAttachmentRef{
		0U, vk::ImageLayout::eColorAttachmentOptimal
	};
	constexpr vk::AttachmentReference DepthAttachmentRef{
		1U, vk::ImageLayout::eDepthStencilAttachmentOptimal
	};
	constexpr vk::AttachmentReference ColorAttachmentResolveRef{
		2U, vk::ImageLayout::eColorAttachmentOptimal
	};

	const vk::SubpassDescription subpassDescription{
		vk::SubpassDescriptionFlags{},
		vk::PipelineBindPoint::eGraphics,
		0,
		nullptr,
		1U,
		&ColorAttachmentRef,
		&ColorAttachmentResolveRef,
		&DepthAttachmentRef,
		0,
		nullptr
	};

	constexpr vk::SubpassDependency Dependency{
		VK_SUBPASS_EXTERNAL,
		0U,
		vk::PipelineStageFlags{ vk::PipelineStageFlagBits::eColorAttachmentOutput |
								vk::PipelineStageFlagBits::eEarlyFragmentTests },
		vk::PipelineStageFlags{ vk::PipelineStageFlagBits::eColorAttachmentOutput |
								vk::PipelineStageFlagBits::eEarlyFragmentTests },
		vk::AccessFlags{},
		vk::AccessFlags{ vk::AccessFlagBits::eColorAttachmentWrite |
						 vk::AccessFlagBits::eDepthStencilAttachmentWrite }
	};

	const vk::RenderPassCreateInfo renderPassInfo{ vk::RenderPassCreateFlags{},
												   static_cast<std::uint32_t>(
													   size(attachments)),
												   attachments.data(),
												   1U,
												   &subpassDescription,
												   1U,
												   &Dependency,
												   nullptr };

	return device.createRenderPass(renderPassInfo);
}

std::tuple<vk::PipelineColorBlendStateCreateInfo, vk::PipelineLayout>
CreatePipelineLayoutInfo(const vk::Device device,
						 const vk::DescriptorSetLayout descriptorSetLayout)
{
	// Band-aid as we need to return address outside of the
	// function...
	constexpr static vk::PipelineColorBlendAttachmentState ColorBlendState{
		VK_FALSE,
		vk::BlendFactor::eOne,
		vk::BlendFactor::eZero,
		vk::BlendOp::eAdd,
		vk::BlendFactor::eOne,
		vk::BlendFactor::eZero,
		vk::BlendOp::eAdd,
		vk::ColorComponentFlags{
			vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
			vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA },
	};

	constexpr vk::PipelineColorBlendStateCreateInfo ColorBlendCreateInfo{
		vk::PipelineColorBlendStateCreateFlags{},
		VK_FALSE,
		vk::LogicOp::eCopy,
		1U,
		&ColorBlendState,
		std::array<float, 4>{ 0.F, 0.F, 0.F, 0.F },
	};

	const vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{
		vk::PipelineLayoutCreateFlags{}, 1U, &descriptorSetLayout, 0U, nullptr,
	};

	return { ColorBlendCreateInfo,
			 device.createPipelineLayout(pipelineLayoutCreateInfo) };
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
	const vk::BufferCreateInfo bufferInfo{
		vk::BufferCreateFlags{},     bufferSize, bufferFlags,
		vk::SharingMode::eExclusive, 0,          nullptr,
	};

	const vk::Buffer deviceBuffer = device.createBuffer(bufferInfo);

	const vk::MemoryRequirements memoryRequirements =
		device.getBufferMemoryRequirements(deviceBuffer);

	const vk::MemoryAllocateInfo allocInfo{
		vk::DeviceSize{ memoryRequirements.size },
		FindMemoryType(physicalDevie, memoryFlags,
					   memoryRequirements.memoryTypeBits),
	};
	const vk::DeviceMemory deviceMemory = device.allocateMemory(allocInfo);
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
							 vk::BufferCopy{ vk::DeviceSize{ 0 },
											 vk::DeviceSize{ 0 },
											 vk::DeviceSize{ size } });
	EndSingleTimeCommands(commandBuffer, graphicsQueue);
}

vk::CommandBuffer BeginSingleTimeCommands(const vk::Device device,
										  const vk::CommandPool commandPool)
{
	const vk::CommandBufferAllocateInfo allocInfo{
		commandPool,
		vk::CommandBufferLevel::ePrimary,
		1,
		nullptr,
	};

	const std::vector<vk::CommandBuffer> commandBuffers =
		device.allocateCommandBuffers(allocInfo);
	assert(commandBuffers.size() == 1);
	constexpr vk::CommandBufferBeginInfo BeginInfo{
		vk::CommandBufferUsageFlags{
			vk::CommandBufferUsageFlagBits::eOneTimeSubmit },
		nullptr, nullptr
	};

	vk::CommandBuffer commandBuffer = commandBuffers.at(0);
	commandBuffer.begin(BeginInfo);
	return commandBuffer;
}

void EndSingleTimeCommands(const vk::CommandBuffer commandBuffer,
						   const vk::Queue queue)
{
	commandBuffer.end();

	const std::array<vk::SubmitInfo, 1> bufferSubmitInfo{ vk::SubmitInfo{
		0, nullptr, nullptr, 1, &commandBuffer, 0, nullptr, nullptr } };
	queue.submit(bufferSubmitInfo);
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

	const vk::BufferImageCopy region{ vk::DeviceSize{ 0 },
									  0U,
									  0U,
									  vk::ImageSubresourceLayers{
										  vk::ImageAspectFlags{
											  vk::ImageAspectFlagBits::eColor },
										  0U,
										  0U,
										  1U,
									  },
									  vk::Offset3D{ 0U, 0U, 0U },
									  vk::Extent3D{ width, height, 1U } };
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
		vk::AccessFlags{},
		vk::AccessFlags{},
		oldLayout,
		newLayout,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		image,
		vk::ImageSubresourceRange{
			vk::ImageAspectFlags{ vk::ImageAspectFlagBits::eColor }, 0U, 1U, 0U,
			1U },
		nullptr,
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
