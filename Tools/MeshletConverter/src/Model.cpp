#include "Model.h"

#include <cstdint>
#include <cassert>

namespace EduEngine::Tools
{
	Model::~Model()
	{
		m_Importer.FreeScene();
	}

	bool Model::Load(const char* filePath)
	{
		m_Scene = m_Importer.ReadFile(filePath, aiProcessPreset_TargetRealtime_Fast | aiProcess_ConvertToLeftHanded);

		if (!m_Scene)
			return false;

		m_Indices.resize(m_Scene->mMeshes[0]->mNumFaces * 3);

		for (uint32_t i = 0; i < m_Scene->mMeshes[0]->mNumFaces; i++)
		{
			auto face = m_Scene->mMeshes[0]->mFaces[i];
			assert(face.mNumIndices == 3);
			m_Indices[i * 3 + 0] = (face.mIndices[0]);
			m_Indices[i * 3 + 1] = (face.mIndices[1]);
			m_Indices[i * 3 + 2] = (face.mIndices[2]);
		}

		return true;
	}
}