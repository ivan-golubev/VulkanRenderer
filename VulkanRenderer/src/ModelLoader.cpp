module;
#include <cassert>
#include <fbxsdk.h>
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

namespace gg
{
	ModelLoader::ModelLoader()
		: mFbxManager{ FbxManager::Create() }
	{
		mFbxManager->SetIOSettings(FbxIOSettings::Create(mFbxManager, IOSROOT));
	}

	ModelLoader::~ModelLoader()
	{
		mFbxManager->Destroy();
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
		FbxImporter* importer = FbxImporter::Create(mFbxManager, "");
		FbxScene* fbxScene = FbxScene::Create(mFbxManager, "");

		bool success = importer->Initialize(modelAbsolutePath.c_str(), -1, mFbxManager->GetIOSettings());
		if (!success) return false;

		success = importer->Import(fbxScene);
		if (!success) return false;

		importer->Destroy();
		FbxNode* rootNode = fbxScene->GetRootNode();

		if (!rootNode)
		{
			fbxScene->Destroy();
			return false;
		}
		for (int i = 0; i < rootNode->GetChildCount(); ++i)
		{
			FbxNode* node = rootNode->GetChild(i);
			FbxMesh* fbxMesh = node->GetMesh();
			
			if (!fbxMesh)
				continue;

			FbxVector4* vertices = fbxMesh->GetControlPoints();
			Mesh mesh{};
			for (int j = 0; j < fbxMesh->GetControlPointsCount(); ++j)
			{
				mesh.Vertices.emplace_back(
					static_cast<float>(vertices[j].mData[0]),
					static_cast<float>(vertices[j].mData[1]),
					static_cast<float>(vertices[j].mData[2]),
					1.0f,
					0.0f, 0.0f, 1.0f, 1.0f
				);
			}
			auto indices = fbxMesh->GetPolygonVertices();
			for (int j = 0; j < fbxMesh->GetPolygonCount(); ++j)
			{
				int iNumVertices = fbxMesh->GetPolygonSize(j);
				BreakIfFalse(iNumVertices == 3);

				for (int k = 0; k < iNumVertices; ++k)
				{
					int controlPointIndex = fbxMesh->GetPolygonVertex(j, k);
					mesh.Indices.push_back(controlPointIndex);
				}
			}
			outModel.meshes.emplace_back(std::move(mesh));
		}
		fbxScene->Destroy();
		return true;
	}

} // namespace gg