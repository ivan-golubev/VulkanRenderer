module;
#include <vector>
export module Model;

import Vertex;

namespace gg
{
	export struct Mesh
	{
		//Mesh(aiMesh const* assimpMesh, aiScene const* scene);
		//Mesh(Mesh&& other) noexcept;

		//~Mesh();

		std::vector<Vertex> Vertices{};
		std::vector<uint32_t> Indices{};
		//std::vector<Color> Colors{};
		//std::vector<TextureCoord> TextureCoords{};
		unsigned char* Texture{ nullptr };
		unsigned int TextureColorFormat{ 0 }; /* GL_RGB or GL_RGBA */
		int TextureWidth{ 0 };
		int TextureHeight{ 0 };

		uint32_t VerticesSizeBytes() const { return static_cast<uint32_t>(Vertices.size()) * sizeof(Vertex); }
		uint32_t IndicesSizeBytes() const { return static_cast<uint32_t>(Indices.size()) * sizeof(uint32_t); }
		//unsigned int ColorsSizeBytes() const { return static_cast<unsigned int>(Vertices.size()) * sizeof(Color); }
		//unsigned int TextureCoordsSizeBytes() const { return static_cast<unsigned int>(Vertices.size()) * sizeof(TextureCoord); }
	private:
		//void ReadVertices(aiMesh const* assimpMesh);
		//void ReadIndices(aiMesh const* assimpMesh);
		//void ReadColors(aiMesh const* assimpMesh, unsigned int setNumber = 0);
		//void ReadTextureCoords(aiMesh const* assimpMesh, aiScene const* scene, unsigned int setNumber = 0);

		//static const inline Color DefaultColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	};

	export struct Model
	{
	public:
		//Model();
		//Model(Model&& other) noexcept;

		//std::shared_ptr<ShaderProgram> shaderProgram{};
		std::vector<Mesh> meshes;
	};

} // namespace gg
