#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace EduEngine::Tools
{
	class Model
	{
	public:
		~Model();

		bool Load(const char* filePath);

		uint32_t* GetIndices() { return m_Indices.data(); }
		size_t GetNumIndices() { return m_Indices.size(); }

		float* GetVertices() { return &m_Scene->mMeshes[0]->mVertices[0].x; }
		uint32_t GetNumVertices() { return m_Scene->mMeshes[0]->mNumVertices; }

		size_t GetVertexStride() const { return sizeof(aiVector3D); }

	private:
		Assimp::Importer m_Importer;
		const aiScene* m_Scene;

		std::vector<uint32_t> m_Indices;
	};
}