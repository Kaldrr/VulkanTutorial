#pragma once
#include <QVector2D>
#include <QVector3D>

#include <array>

#include <vulkan/vulkan.hpp>

struct Vertex
{
	QVector3D Position;
	QVector3D Color;
	QVector2D TextureCoordinate;

	static_assert(sizeof(QVector2D) == sizeof(std::array<float, 2>));
	static_assert(sizeof(QVector3D) == sizeof(std::array<float, 3>));

	[[nodiscard]] consteval static vk::VertexInputBindingDescription
	GetBindingDescription() noexcept
	{
		constexpr vk::VertexInputBindingDescription BindingDescription{
			.binding   = 0U,
			.stride    = sizeof(Vertex),
			.inputRate = vk::VertexInputRate::eVertex
		};

		return BindingDescription;
	}

	[[nodiscard]] consteval static auto GetAttributeDescriptions() noexcept
	{
		constexpr std::array<vk::VertexInputAttributeDescription, 3>
			AttributeDescriptions{
			    // Position description
				vk::VertexInputAttributeDescription{
					.location = 0,
					.binding  = 0,
					.format   = vk::Format::eR32G32B32Sfloat,
					.offset   = offsetof(Vertex, Position),
				},
			    // Color description
			    vk::VertexInputAttributeDescription{
					.location = 1,
					.binding  = 0,
					.format   = vk::Format::eR32G32B32Sfloat,
					.offset   = offsetof(Vertex, Color),
				},
			    // Texture description
			    vk::VertexInputAttributeDescription{
					.location = 2,
					.binding  = 0,
					.format   = vk::Format::eR32G32Sfloat,
					.offset   = offsetof(Vertex, TextureCoordinate),
				},
		    };

		return AttributeDescriptions;
	}
};
