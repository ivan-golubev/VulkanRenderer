module;
#include <DirectXMath.h>
export module Vertex;

using DirectX::XMVECTOR;

namespace awesome::structs {

	export struct Vertex
	{
		XMVECTOR Position;
		XMVECTOR Color;

		Vertex(float x, float y, float z, float w, float r, float g, float b, float a)
			: Position{x, y, z, w}
			, Color{r, g, b, a}
		{}
	};

}
