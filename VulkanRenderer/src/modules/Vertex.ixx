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

		Vertex(float x, float y, float z, float w, XMFLOAT2 TexCoords0)
			: Position{ x, y, z, w }
			, TextureCoords0{ TexCoords0 }
		{}

		static VkVertexInputBindingDescription GetBindingDescription();
		static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions();
	};

} // namespace gg
