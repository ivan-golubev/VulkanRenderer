module;
#include <cassert>
#include <DirectXMath.h>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <vector>
module ModelLoader;

import Logging;
import Model;
import ShaderProgram;
import ErrorHandling;

using DirectX::XMFLOAT2;

namespace
{
	using namespace gg;
}

namespace gg
{
	ModelLoader::ModelLoader()
	{
	}

	ModelLoader::~ModelLoader()
	{
	}

	std::unique_ptr<Model> ModelLoader::LoadModel(std::string const& modelRelativePath, std::string const& vertexShaderRelativePath, std::string const& fragmentShaderRelativePath)
	{
		std::string modelFileAbsPath{ std::filesystem::absolute(modelRelativePath).generic_string() };
		std::string vertexShaderAbsPath{ std::filesystem::absolute(vertexShaderRelativePath).generic_string() };
		std::string fragmentShaderAbsPath{ std::filesystem::absolute(fragmentShaderRelativePath).generic_string() };
		std::unique_ptr<Model> model = std::make_unique<Model>();
		model->shaderProgram = make_shared<ShaderProgram>(vertexShaderAbsPath, fragmentShaderAbsPath);

		if (!LoadMeshes(modelFileAbsPath, *model))
			throw std::runtime_error(std::format("Failed to read the input model: {}", modelFileAbsPath));
		return model;
	}

	bool ModelLoader::LoadMeshes(std::string const& modelAbsolutePath, Model& outModel)
	{
		return false;
	}

} // namespace gg