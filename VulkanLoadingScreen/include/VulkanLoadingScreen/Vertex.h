#pragma once
#include <QVector2D>
#include <QVector3D>

#include <array>

#include <vulkan/vulkan.hpp>

struct Vertex
{
	QVector3D m_Position{};
	QVector3D m_Color{};
	QVector2D m_TextureCoordinate{};

	static_assert(sizeof(QVector2D) == sizeof(std::array<float, 2>));
	static_assert(sizeof(QVector3D) == sizeof(std::array<float, 3>));

	[[nodiscard]] constexpr static vk::VertexInputBindingDescription
	getBindingDescription() noexcept
	{
		constexpr vk::VertexInputBindingDescription bindingDescription{
			0u,
			sizeof(Vertex),
			vk::VertexInputRate::eVertex,
		};

		return bindingDescription;
	}

	[[nodiscard]] constexpr static auto getAttributeDescriptions() noexcept
	{
		constexpr static std::array<vk::VertexInputAttributeDescription,
		                            3>
		    attributeDescriptions{
			    // Position description
			    vk::VertexInputAttributeDescription{
			        0,
			        0,
			        vk::Format::eR32G32B32Sfloat,
			        offsetof(Vertex, m_Position),
			    },
			    // Color description
			    vk::VertexInputAttributeDescription{
			        1,
			        0,
			        vk::Format::eR32G32B32Sfloat,
			        offsetof(Vertex, m_Color),
			    },
			    // Texture description
			    vk::VertexInputAttributeDescription{
			        2,
			        0,
			        vk::Format::eR32G32Sfloat,
			        offsetof(Vertex, m_TextureCoordinate),
			    },
		    };

		return std::span{ attributeDescriptions };
	}
};
