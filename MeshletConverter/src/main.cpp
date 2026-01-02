#include <stdio.h>
#include <meshoptimizer.h>
#include <fstream>
#include <string>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

static constexpr uint32_t PROLOG = 'M' | ('S' << 8) | ('H' << 16) | ('L' << 24);

enum FileVersion
{
	FILE_VERSION_0,
};

struct FileHeader
{
	uint32_t prolog = PROLOG;
	uint32_t version = FILE_VERSION_0;
	uint32_t meshlets_count;
	uint32_t vertices_count;
	uint32_t triangles_count;
};

int main(int argc, void* argv[])
{
	if (argc <= 1)
	{
		printf("The file path argument was not found");
		return 0;
	}

	const char* modelPath = static_cast<const char*>(argv[1]);

	Assimp::Importer assimpImporter;
	auto scene = assimpImporter.ReadFile(modelPath, aiProcessPreset_TargetRealtime_Fast | aiProcess_ConvertToLeftHanded);

	if (!scene)
	{
		printf("Failed to read file: %s\n", modelPath);
		assimpImporter.FreeScene();
		return 0;
	}

	std::vector<uint32_t> indices(scene->mMeshes[0]->mNumFaces * 3);

	for (uint32_t i = 0; i < scene->mMeshes[0]->mNumFaces; i++)
	{
		auto face = scene->mMeshes[0]->mFaces[i];
		assert(face.mNumIndices == 3);
		indices[i * 3 + 0] = (face.mIndices[0]);
		indices[i * 3 + 1] = (face.mIndices[1]);
		indices[i * 3 + 2] = (face.mIndices[2]);
	}

	const size_t max_vertices = 64;
	const size_t max_triangles = 126;
	const float cone_weight = 0.0f;

	size_t max_meshlets = meshopt_buildMeshletsBound(indices.size(), max_vertices, max_triangles);
	std::vector<meshopt_Meshlet> meshlets(max_meshlets);
	std::vector<uint32_t> meshlet_vertices(indices.size());
	std::vector<uint8_t> meshlet_triangles(indices.size());

	size_t meshlet_count = meshopt_buildMeshlets(
		meshlets.data(),
		meshlet_vertices.data(),
		meshlet_triangles.data(),
		indices.data(),
		indices.size(),
		&scene->mMeshes[0]->mVertices[0].x,
		scene->mMeshes[0]->mNumVertices,
		sizeof(aiVector3D),
		max_vertices,
		max_triangles,
		cone_weight
	);

	const meshopt_Meshlet& last = meshlets[meshlet_count - 1];

	meshlet_vertices.resize(last.vertex_offset + last.vertex_count);
	meshlet_triangles.resize(last.triangle_offset + last.triangle_count * 3);
	meshlets.resize(meshlet_count);

	assert(meshlet_triangles.size() % 3 == 0);

	for (auto& m : meshlets)
		meshopt_optimizeMeshlet(&meshlet_vertices[m.vertex_offset], &meshlet_triangles[m.triangle_offset], m.triangle_count, m.vertex_count);

	for (auto& m : meshlets)
		m.triangle_offset /= 3;

	std::vector<uint32_t> meshlet_triangles_packed(meshlet_triangles.size() / 3);
	for (size_t i = 0; i < meshlet_triangles_packed.size(); i++)
	{
		meshlet_triangles_packed[i] = (meshlet_triangles[i * 3 + 0] << 0)  |
									  (meshlet_triangles[i * 3 + 1] << 10) |
									  (meshlet_triangles[i * 3 + 2] << 20);
	}

	FileHeader fh = {};
	fh.meshlets_count = meshlet_count;
	fh.vertices_count = meshlet_vertices.size();
	fh.triangles_count = meshlet_triangles_packed.size();

	std::string outPath = modelPath;

	auto dot = outPath.find_last_of('.');
	if (dot != std::string::npos)
		outPath.resize(dot);

	outPath += ".mshl";

	std::ofstream ostream(outPath.c_str(), std::ios::binary);

	ostream.write(reinterpret_cast<char*>(&fh), sizeof(FileHeader));
	ostream.write(reinterpret_cast<char*>(meshlets.data()), sizeof(meshopt_Meshlet) * meshlet_count);
	ostream.write(reinterpret_cast<char*>(meshlet_vertices.data()), sizeof(uint32_t) * meshlet_vertices.size());
	ostream.write(reinterpret_cast<char*>(meshlet_triangles_packed.data()), sizeof(uint32_t) * meshlet_triangles_packed.size());

	ostream.close();

	assimpImporter.FreeScene();

	printf("Successful create %s\n", outPath.c_str());
	printf("Meshlet count: %d\nVertices count: %d\nTriangle count: %d\n", fh.meshlets_count, fh.vertices_count, fh.triangles_count);

	return 0;
}