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

		Vertex(float x, float y, float z, float w)
			: Position{x, y, z, w}
		{}

		static VkVertexInputBindingDescription GetBindingDescription();
		static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions();
	};

} // namespace gg
