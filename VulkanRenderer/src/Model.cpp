module;
#include <cstdint>
#include <memory>
module Model;

namespace gg
{
	uint32_t Mesh::VerticesSizeBytes() const { return static_cast<uint32_t>(Vertices.size()) * sizeof(Vertex); }
	uint32_t Mesh::IndicesSizeBytes() const { return static_cast<uint32_t>(Indices.size()) * sizeof(uint32_t); }
	uint32_t Mesh::GetVertexCount() const { return static_cast<uint32_t>(Vertices.size()); }
	uint32_t Mesh::GetIndexCount() const { return static_cast<uint32_t>(Indices.size()); }

	Mesh::Mesh(Mesh&& other) noexcept
		: Vertices{ std::move(other.Vertices) }
		, Indices{ std::move(other.Indices) }
	{
	}

	Mesh& Mesh::operator=(Mesh&& other) noexcept
	{
		if (this != &other)
		{
			Vertices.clear();
			Indices.clear();
			Vertices = std::move(other.Vertices);
			Indices = std::move(other.Indices);
		}
		return *this;
	}

	Model::Model(Model&& other) noexcept
		: shaderProgram{ other.shaderProgram }
		, meshes{ std::move(other.meshes) }
	{
	}

	Model& Model::operator=(Model&& other) noexcept
	{
		if (this != &other)
		{
			meshes.clear();

			shaderProgram = std::move(other.shaderProgram);
			meshes = std::move(other.meshes);
		}
		return *this;
	}

} // namespace gg
