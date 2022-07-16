module;
#include <DirectXMath.h>
#include <array>
#include <vulkan/vulkan.h>
export module Vertex;

using DirectX::XMVECTOR;
using DirectX::XMFLOAT2;

namespace gg 
{
	export struct Vertex
	{
		XMVECTOR Position;
		XMFLOAT2 TextureCoords0{};

		Vertex(float x, float y, float z, float w)
			: Position{x, y, z, w}
		{}

		Vertex(float x, float y, float z, float w, float u, float v)
			: Position{ x, y, z, w }
			, TextureCoords0{ u, v }
		{}

		static VkVertexInputBindingDescription GetBindingDescription();
		static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions();
	};

} // namespace gg
