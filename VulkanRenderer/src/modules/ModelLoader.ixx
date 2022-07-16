module;
#include <memory>
#include <string>
export module ModelLoader;

import Model;

namespace gg
{
	export class ModelLoader
	{
	public:
		ModelLoader();
		~ModelLoader();

		std::unique_ptr<Model> LoadModel(std::string const& modelRelativePath, std::string const& vertexShaderRelativePath, std::string const& fragmentShaderRelativePath);

	private:
		bool LoadMeshes(std::string const& modelAbsolutePath, Model & outModel);
	};

} // namespace gg