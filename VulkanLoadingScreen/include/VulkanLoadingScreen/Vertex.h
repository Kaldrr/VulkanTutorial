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

	[[nodiscard]] constexpr static vk::VertexInputBindingDescription
	GetBindingDescription() noexcept
	{
		constexpr vk::VertexInputBindingDescription BindingDescription{
			0U, sizeof(Vertex), vk::VertexInputRate::eVertex
		};

		return BindingDescription;
	}

	[[nodiscard]] static auto GetAttributeDescriptions() noexcept
	{
		const static std::array<vk::VertexInputAttributeDescription, 3>
			AttributeDescriptions{
			    // Position description
				vk::VertexInputAttributeDescription{ 0, 0,
													 vk::Format::eR32G32B32Sfloat,
													 offsetof(Vertex, Position) },
			    // Color description
			    vk::VertexInputAttributeDescription{
					1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, Color) },
			    // Texture description
			    vk::VertexInputAttributeDescription{
					2, 0, vk::Format::eR32G32Sfloat,
					offsetof(Vertex, TextureCoordinate) },
		    };

		return std::span{ AttributeDescriptions };
	}
};
