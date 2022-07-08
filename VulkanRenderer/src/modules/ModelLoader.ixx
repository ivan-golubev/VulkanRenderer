module;
#include <string>
#include <fbxsdk.h>
export module ModelLoader;

import Model;

namespace gg
{
	export class ModelLoader
	{
	public:
		ModelLoader();
		~ModelLoader();

		Model LoadModel(std::string const& modelRelativePath, std::string const& vertexShaderRelativePath, std::string const& fragmentShaderRelativePath);

	private:
		bool LoadShaders(std::string const& vertexShaderAbsolutePath, std::string const& fragmentShaderAbsolutePath);
		bool LoadMeshes(std::string const& modelAbsolutePath, Model & outModel);

		FbxManager* mFbxManager;
	};

} // namespace gg