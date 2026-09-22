#include "TestAsserts.h"

#include <ShaderCache.h>
#include <ShaderResources.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace EduEngine::EduBinding;
using EduEngine::Tests::AssertCapture;

namespace
{
	constexpr const wchar_t* LayoutShader = L"assets/Shaders/ResourceLayoutCS.hlsl";
	constexpr const wchar_t* BindlessShader = L"assets/Shaders/BindlessCS.hlsl";
	constexpr const wchar_t* Target = L"cs_6_6";

	typedef uint16 (ShaderResources::*CountGetter)(SHADER_RESOURCE_TYPE) const;
	typedef ShaderResourceInfo& (ShaderResources::*ResourceGetter)(SHADER_RESOURCE_TYPE, uint16) const;

	struct Category
	{
		const char*	   Name;
		CountGetter	   GetCount;
		ResourceGetter GetResource;
	};

	const Category Categories[] =
	{
		{ "CB",		  &ShaderResources::GetCBNum,	   &ShaderResources::GetCB },
		{ "TexSRV",	  &ShaderResources::GetTexSRVNum,  &ShaderResources::GetTexSRV },
		{ "BuffSRV",  &ShaderResources::GetBuffSRVNum, &ShaderResources::GetBuffSRV },
		{ "TexUAV",	  &ShaderResources::GetTexUAVNum,  &ShaderResources::GetTexUAV },
		{ "BuffUAV",  &ShaderResources::GetBuffUAVNum, &ShaderResources::GetBuffUAV },
	};

	/// Names of the resources of one category, sorted, so that the expected values
	/// do not depend on the order the reflection reports the resources in
	std::vector<std::string> CollectNames(const ShaderResources& resources, const Category& category, SHADER_RESOURCE_TYPE type)
	{
		std::vector<std::string> names;

		for (uint16 i = 0; i < (resources.*category.GetCount)(type); i++)
			names.push_back((resources.*category.GetResource)(type, i).GetName());

		std::sort(names.begin(), names.end());

		return names;
	}

	const ShaderResourceInfo* FindResource(const ShaderResources& resources, const char* name)
	{
		for (uint16 type = 0; type < SHADER_RESOURCE_TYPE_NUM; type++)
		{
			for (const Category& category : Categories)
			{
				SHADER_RESOURCE_TYPE resType = static_cast<SHADER_RESOURCE_TYPE>(type);

				for (uint16 i = 0; i < (resources.*category.GetCount)(resType); i++)
				{
					const ShaderResourceInfo& resource = (resources.*category.GetResource)(resType, i);

					if (std::string(resource.GetName()) == name)
						return &resource;
				}
			}
		}

		return nullptr;
	}

	uint16 GetTotalResourcesNum(const ShaderResources& resources, SHADER_RESOURCE_TYPE type)
	{
		uint16 total = 0;

		for (const Category& category : Categories)
			total += (resources.*category.GetCount)(type);

		return total;
	}
}

/// Checks how EduBinding turns D3D12 shader reflection into its own resource layout.
/// The shaders are really compiled with DXC, but no GPU is needed.
class ShaderResourcesTests : public testing::Test
{
protected:
	ShaderResources* GetResources(const wchar_t* path, const ShaderDesc& desc = ShaderDesc())
	{
		auto shader = m_Cache.GetOrCreate(path, L"CSMain", Target, nullptr, desc);

		if (shader == nullptr)
			return nullptr;

		m_Shaders.push_back(shader);
		return shader->GetResources();
	}

	void ExpectNames(const ShaderResources&				resources,
					 SHADER_RESOURCE_TYPE				type,
					 const char*						categoryName,
					 const std::vector<std::string>&	expected)
	{
		auto it = std::find_if(std::begin(Categories), std::end(Categories),
							   [categoryName](const Category& c) { return std::string(c.Name) == categoryName; });

		ASSERT_NE(it, std::end(Categories));
		EXPECT_EQ(CollectNames(resources, *it, type), expected) << categoryName << " of " << (type == SHADER_RESOURCE_TYPE_DYNAMIC ? "dynamic" : "mutable") << " resources";
	}

	ShaderCache m_Cache;
	std::vector<std::shared_ptr<ShaderD3D12>> m_Shaders;
};

TEST_F(ShaderResourcesTests, SortsResourcesIntoCategories)
{
	ShaderResources* resources = GetResources(LayoutShader);
	ASSERT_NE(resources, nullptr);

	// Constant buffers, textures, buffer SRVs, texture UAVs and buffer UAVs are kept apart,
	// no matter how they are declared in HLSL
	ExpectNames(*resources, SHADER_RESOURCE_TYPE_MUTABLE, "CB", { "cbObject", "cbPass" });
	ExpectNames(*resources, SHADER_RESOURCE_TYPE_MUTABLE, "TexSRV", { "gAlbedo", "gEnvironment", "gNormal", "gTextureArray", "gUnboundedArray" });
	ExpectNames(*resources, SHADER_RESOURCE_TYPE_MUTABLE, "BuffSRV", { "gInstances", "gRawData", "gTypedBuffer" });
	ExpectNames(*resources, SHADER_RESOURCE_TYPE_MUTABLE, "TexUAV", { "gOutputTexture" });
	ExpectNames(*resources, SHADER_RESOURCE_TYPE_MUTABLE, "BuffUAV", { "gRawOutput", "gResults", "gTypedOutput" });

	// Everything is mutable by default
	EXPECT_EQ(GetTotalResourcesNum(*resources, SHADER_RESOURCE_TYPE_DYNAMIC), 0);
}

TEST_F(ShaderResourcesTests, SplitsResourcesByShaderResourceType)
{
	ShaderResourceDesc dynamicResources[] =
	{
		{ "cbPass", SHADER_RESOURCE_TYPE_DYNAMIC },
		{ "gAlbedo", SHADER_RESOURCE_TYPE_DYNAMIC },
		{ "gResults", SHADER_RESOURCE_TYPE_DYNAMIC },
	};

	ShaderDesc desc;
	desc.ResourceNum = _countof(dynamicResources);
	desc.ResourceDesc = dynamicResources;

	ShaderResources* resources = GetResources(LayoutShader, desc);
	ASSERT_NE(resources, nullptr);

	ExpectNames(*resources, SHADER_RESOURCE_TYPE_DYNAMIC, "CB", { "cbPass" });
	ExpectNames(*resources, SHADER_RESOURCE_TYPE_DYNAMIC, "TexSRV", { "gAlbedo" });
	ExpectNames(*resources, SHADER_RESOURCE_TYPE_DYNAMIC, "BuffUAV", { "gResults" });
	EXPECT_EQ(GetTotalResourcesNum(*resources, SHADER_RESOURCE_TYPE_DYNAMIC), 3);

	ExpectNames(*resources, SHADER_RESOURCE_TYPE_MUTABLE, "CB", { "cbObject" });
	ExpectNames(*resources, SHADER_RESOURCE_TYPE_MUTABLE, "TexSRV", { "gEnvironment", "gNormal", "gTextureArray", "gUnboundedArray" });
	ExpectNames(*resources, SHADER_RESOURCE_TYPE_MUTABLE, "BuffUAV", { "gRawOutput", "gTypedOutput" });
	EXPECT_EQ(GetTotalResourcesNum(*resources, SHADER_RESOURCE_TYPE_MUTABLE), 11);

	// Every resource knows the type it was sorted into
	for (uint16 type = 0; type < SHADER_RESOURCE_TYPE_NUM; type++)
	{
		SHADER_RESOURCE_TYPE resType = static_cast<SHADER_RESOURCE_TYPE>(type);

		for (const Category& category : Categories)
		{
			for (uint16 i = 0; i < (resources->*category.GetCount)(resType); i++)
			{
				const ShaderResourceInfo& resource = (resources->*category.GetResource)(resType, i);
				EXPECT_EQ(resource.GetResType(), resType) << category.Name << " \"" << resource.GetName() << "\"";
			}
		}
	}
}

TEST_F(ShaderResourcesTests, DefaultTypeAppliesToResourcesNotListedInDesc)
{
	ShaderResourceDesc mutableResources[] = { { "gAlbedo", SHADER_RESOURCE_TYPE_MUTABLE } };

	ShaderDesc desc;
	desc.ResourceNum = _countof(mutableResources);
	desc.ResourceDesc = mutableResources;
	desc.DefaultType = SHADER_RESOURCE_TYPE_DYNAMIC;

	ShaderResources* resources = GetResources(LayoutShader, desc);
	ASSERT_NE(resources, nullptr);

	ExpectNames(*resources, SHADER_RESOURCE_TYPE_MUTABLE, "TexSRV", { "gAlbedo" });
	EXPECT_EQ(GetTotalResourcesNum(*resources, SHADER_RESOURCE_TYPE_MUTABLE), 1);
	EXPECT_EQ(GetTotalResourcesNum(*resources, SHADER_RESOURCE_TYPE_DYNAMIC), 13);
}

TEST_F(ShaderResourcesTests, KeepsBindPointSpaceAndCount)
{
	ShaderResources* resources = GetResources(LayoutShader);
	ASSERT_NE(resources, nullptr);

	const ShaderResourceInfo* albedo = FindResource(*resources, "gAlbedo");
	ASSERT_NE(albedo, nullptr);
	EXPECT_EQ(albedo->GetBindPoint(), 0);
	EXPECT_EQ(albedo->GetSpace(), 0);
	EXPECT_EQ(albedo->GetBindCount(), 1);
	EXPECT_EQ(albedo->GetInputType(), D3D_SIT_TEXTURE);
	EXPECT_EQ(albedo->GetSRVDim(), D3D_SRV_DIMENSION_TEXTURE2D);

	const ShaderResourceInfo* cbObject = FindResource(*resources, "cbObject");
	ASSERT_NE(cbObject, nullptr);
	EXPECT_EQ(cbObject->GetBindPoint(), 1);
	EXPECT_EQ(cbObject->GetInputType(), D3D_SIT_CBUFFER);

	// A bounded array takes several consecutive bind points
	const ShaderResourceInfo* textureArray = FindResource(*resources, "gTextureArray");
	ASSERT_NE(textureArray, nullptr);
	EXPECT_EQ(textureArray->GetBindPoint(), 3);
	EXPECT_EQ(textureArray->GetSpace(), 0);
	EXPECT_EQ(textureArray->GetBindCount(), 4);

	const ShaderResourceInfo* unbounded = FindResource(*resources, "gUnboundedArray");
	ASSERT_NE(unbounded, nullptr);
	EXPECT_EQ(unbounded->GetBindPoint(), 0);
	EXPECT_EQ(unbounded->GetSpace(), 1);

	const ShaderResourceInfo* typedBuffer = FindResource(*resources, "gTypedBuffer");
	ASSERT_NE(typedBuffer, nullptr);
	EXPECT_EQ(typedBuffer->GetInputType(), D3D_SIT_TEXTURE);
	EXPECT_EQ(typedBuffer->GetSRVDim(), D3D_SRV_DIMENSION_BUFFER);
}

TEST_F(ShaderResourcesTests, DoesNotTrackSamplers)
{
	ShaderResources* resources = GetResources(LayoutShader);
	ASSERT_NE(resources, nullptr);

	EXPECT_EQ(FindResource(*resources, "gSampler"), nullptr);
	EXPECT_EQ(GetTotalResourcesNum(*resources, SHADER_RESOURCE_TYPE_MUTABLE), 14);
}

TEST_F(ShaderResourcesTests, DoesNotTrackResourcesReachedThroughDescriptorHeap)
{
	ShaderResources* resources = GetResources(BindlessShader);
	ASSERT_NE(resources, nullptr);

	// Bindless resources are indexed in the shader itself, so reflection only
	// reports the constant buffer that holds their indices
	ExpectNames(*resources, SHADER_RESOURCE_TYPE_MUTABLE, "CB", { "cbIndices" });
	EXPECT_EQ(GetTotalResourcesNum(*resources, SHADER_RESOURCE_TYPE_MUTABLE), 1);
	EXPECT_EQ(GetTotalResourcesNum(*resources, SHADER_RESOURCE_TYPE_DYNAMIC), 0);
}

TEST_F(ShaderResourcesTests, ReportsOutOfRangeIndex)
{
	ShaderResources* resources = GetResources(LayoutShader);
	ASSERT_NE(resources, nullptr);

	for (const Category& category : Categories)
	{
		SCOPED_TRACE(category.Name);

		uint16 count = (resources->*category.GetCount)(SHADER_RESOURCE_TYPE_MUTABLE);
		ASSERT_GT(count, 0);

		{
			AssertCapture asserts;
			(resources->*category.GetResource)(SHADER_RESOURCE_TYPE_MUTABLE, count - 1);
			EXPECT_TRUE(asserts.GetMessages().empty()) << "The last valid index must be accepted";
		}

#if defined(DEBUG) | defined(_DEBUG)
		{
			AssertCapture asserts;
			(resources->*category.GetResource)(SHADER_RESOURCE_TYPE_MUTABLE, count);
			EXPECT_FALSE(asserts.GetMessages().empty()) << "Index past the last resource must be reported";
		}
#endif
	}
}
