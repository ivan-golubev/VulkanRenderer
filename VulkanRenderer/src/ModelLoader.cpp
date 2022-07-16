module;
#include <cassert>
#include <DirectXMath.h>
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

using DirectX::XMFLOAT2;

namespace
{
	using namespace gg;
	std::vector<XMFLOAT2> readTextureCoords(FbxMesh* fbxMesh)
	{
		std::vector<XMFLOAT2> result{};

		/* Get all UV set names */
		FbxStringList uvSetNames;
		fbxMesh->GetUVSetNames(uvSetNames);
		for (int i = 0; i < uvSetNames.GetCount(); ++i)
		{
			char const* setName = uvSetNames.GetStringAt(i);
			FbxGeometryElementUV const* uvElement = fbxMesh->GetElementUV(setName);
			if (!uvElement)
				continue;
			
			/* only support mapping mode eByPolygonVertex and eByControlPoint */
			if (uvElement->GetMappingMode() != FbxGeometryElement::eByPolygonVertex
			 && uvElement->GetMappingMode() != FbxGeometryElement::eByControlPoint)
				return result;

			auto mapping = uvElement->GetMappingMode();

			/* index array, which holds the index referenced to the UV data */
			bool const useIndex = uvElement->GetReferenceMode() != FbxGeometryElement::eDirect;
			int const indexCount = (useIndex) ? uvElement->GetIndexArray().GetCount() : 0;
			/* iterating through the data by polygon */
			if (uvElement->GetMappingMode() == FbxGeometryElement::eByControlPoint)
			{
				for (int polyIndex = 0; polyIndex < fbxMesh->GetPolygonCount(); ++polyIndex)
				{
					/* build the max index array that we need to pass into MakePoly */
					int const polySize = fbxMesh->GetPolygonSize(polyIndex);
					for (int vertIx = 0; vertIx < polySize; ++vertIx)
					{
						FbxVector2 uvValue;
						/* get the index of the current vertex in control points array */
						int polyVertIx = fbxMesh->GetPolygonVertex(polyIndex, vertIx);
						/* the UV index depends on the reference mode */
						int uvIndex = useIndex ? uvElement->GetIndexArray().GetAt(polyVertIx) : polyVertIx;
						uvValue = uvElement->GetDirectArray().GetAt(uvIndex);

						/* just support one UV set for the first iteration */
						result.emplace_back(
							static_cast<float>(uvValue.mData[0]),
							static_cast<float>(uvValue.mData[1])
						);
					}
				}
			}
			else if (uvElement->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
			{
				int polyIndexCounter = 0;
				for (int polyIndex = 0; polyIndex < fbxMesh->GetPolygonCount(); ++polyIndex)
				{
					/* build the max index array that we need to pass into MakePoly */
					int const polySize = fbxMesh->GetPolygonSize(polyIndex);
					for (int vertIx = 0; vertIx < polySize; ++vertIx)
					{
						if (polyIndexCounter < indexCount)
						{
							FbxVector2 uvValue;
							/* the UV index depends on the reference mode */
							int uvIndex = useIndex ? uvElement->GetIndexArray().GetAt(polyIndexCounter) : polyIndexCounter;
							uvValue = uvElement->GetDirectArray().GetAt(uvIndex);
						
							/* just support one UV set for the first iteration */
							result.emplace_back(
								static_cast<float>(uvValue.mData[0]),
								static_cast<float>(uvValue.mData[1])
							);

							polyIndexCounter++;
						}
					}
				}
			}
			/* just support one UV set for the first iteration */
			break;
		}
		return result;
	}

	/* unique vertices, indices set */
	Mesh readMeshIndexed(FbxMesh* fbxMesh)
	{
		FbxVector4* vertices = fbxMesh->GetControlPoints();
		Mesh mesh{};
		for (int j = 0; j < fbxMesh->GetControlPointsCount(); ++j)
		{
			mesh.Vertices.emplace_back(
				static_cast<float>(vertices[j].mData[0]),
				static_cast<float>(vertices[j].mData[1]),
				static_cast<float>(vertices[j].mData[2]),
				1.0f // w coordinate
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
		return mesh;
	}

	/* repeated vertices, textured meshes */
	Mesh readMeshTextured(FbxMesh* fbxMesh, std::vector<XMFLOAT2>& textureCoordinates)
	{
		Mesh mesh{}; // TODO;
		//for (int polyIndex = 0; polyIndex < fbxMesh->GetPolygonVertexCount(); ++polyIndex)
		//{
		//	fbxMesh->GetPolygonVertices()[]

		//	int const polySize = fbxMesh->GetPolygonSize(polyIndex);
		//	for (int vertIx = 0; vertIx < polySize; ++vertIx)
		//	{
		//		fbxMesh->GetPolygonCount()
		//	}
		//}
		//return mesh;
	}
}

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

			std::vector<XMFLOAT2> textureCoordinates0 = readTextureCoords(fbxMesh);
			outModel.meshes.emplace_back(readMeshTextured(fbxMesh, textureCoordinates0));
		}
		fbxScene->Destroy();
		return true;
	}

} // namespace gg