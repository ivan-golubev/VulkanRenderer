module;
#include <string>
#include <vector>
#include <memory>
#include <cassert>
#include <filesystem>
#include <format>
#include <fbxsdk.h>
module ModelLoader;

import Logging;
import Model;

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

	Model ModelLoader::LoadModel(std::string const& modelRelativePath, std::string const& vertexShaderRelativePath, std::string const& fragmentShaderRelativePath)
	{
		std::string modelFileAbsPath{ std::filesystem::absolute(modelRelativePath).generic_string() };
		std::string vertexShaderAbsPath{ std::filesystem::absolute(vertexShaderRelativePath).generic_string() };
		std::string fragmentShaderAbsPath{ std::filesystem::absolute(fragmentShaderRelativePath).generic_string() };
		Model model{};

		bool result{ LoadShaders(vertexShaderAbsPath, fragmentShaderAbsPath) };
		if (!result)
		{
			DebugLog(DebugLevel::Error, std::format("Failed to read input shaders - VS: {} PS: {}", vertexShaderAbsPath, fragmentShaderAbsPath));
			//std::cout << importer.GetErrorString() << modelFileAbsPath << std::endl;
			exit(-1);
		}
		result = LoadMeshes(modelFileAbsPath, model);
		if (!result)
		{
			DebugLog(DebugLevel::Error, std::format("Failed to read the input model: {}", modelFileAbsPath));
			//std::cout << importer.GetErrorString() << modelFileAbsPath << std::endl;
			exit(-1);
		}
		return model;
	}

	bool ModelLoader::LoadShaders(std::string const& vertexShaderAbsolutePath, std::string const& fragmentShaderAbsolutePath)
	{
		//shaderProgram = std::make_shared<ShaderProgram>(vertexShaderAbsPath, fragmentShaderAbsPath);

		//temp hack
		return true;

		//if (!shaderProgram->LinkedSuccessfully())
		//{
		//	std::cout << "Failed to initialize the shaders" << std::endl;
		//	exit(-1);
		//}
		return false;
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
				auto fbxColor = fbxMesh->GetElementVertexColor(j);
				mesh.Vertices.emplace_back(
					static_cast<float>(vertices[j].mData[0]),
					static_cast<float>(vertices[j].mData[1]),
					static_cast<float>(vertices[j].mData[2]),
					1.0f,
					0.0f, 0.0f, 0.0f, 0.0f
	/*				fbxColor->mRed,
					fbxColor->mGreen,
					fbxColor->mBlue,
					fbxColor->mAlpha*/
				);
			}
			auto indices = fbxMesh->GetPolygonVertices();
			//std::vector<uint32_t>(indices, indices + fbxMesh->GetPolygonVertexCount());
			/*for (int j = 0; j < fbxMesh->GetPolygonVertexCount(); ++j)
			{
				mesh.Indices.push_back(indices[j]);
			}*/
			for (int j = 0; j < fbxMesh->GetPolygonCount(); ++j)
			{
				int iNumVertices = fbxMesh->GetPolygonSize(j);
				assert(iNumVertices == 3);

				for (int k = 0; k < iNumVertices; ++k)
				{
					int controlPointIndex = fbxMesh->GetPolygonVertex(j, k);
					mesh.Indices.push_back(controlPointIndex);
				}
			}
			outModel.meshes.push_back(mesh);
		}
		fbxScene->Destroy();
		return true;
	}

} // namespace gg