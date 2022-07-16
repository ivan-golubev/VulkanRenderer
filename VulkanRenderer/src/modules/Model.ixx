module;
#include <memory>
#include <vector>
export module Model;

import Vertex;
import ShaderProgram;

namespace gg
{
	export struct Mesh
	{
		Mesh() = default;
		Mesh(Mesh&&) noexcept;
		Mesh& operator=(Mesh&&) noexcept;

		std::vector<Vertex> Vertices{};
		std::vector<uint32_t> Indices{};

		unsigned char* Texture{ nullptr };
		unsigned int TextureColorFormat{ 0 }; /* GL_RGB or GL_RGBA */
		int TextureWidth{ 0 };
		int TextureHeight{ 0 };

		uint32_t VerticesSizeBytes() const;
		uint32_t IndicesSizeBytes() const;
		uint32_t GetVertexCount() const;
		uint32_t GetIndexCount() const;
	};

	export struct Model
	{
	public:
		Model() = default;
		Model(Model&&) noexcept;
		Model& operator=(Model&&) noexcept;

		std::shared_ptr<ShaderProgram> shaderProgram;
		std::vector<Mesh> meshes;
	};

} // namespace gg
