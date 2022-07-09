module;
#include <DirectXMath.h>
#include <array>
#include <vulkan/vulkan.h>
export module Vertex;

using DirectX::XMVECTOR;

namespace gg 
{
	export struct Vertex
	{
		XMVECTOR Position;
		XMVECTOR Color;

		Vertex(float x, float y, float z, float w, float r, float g, float b, float a)
			: Position{x, y, z, w}
			, Color{r, g, b, a}
		{}

		static VkVertexInputBindingDescription GetBindingDescription();
		static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions();
	};

} // namespace gg
