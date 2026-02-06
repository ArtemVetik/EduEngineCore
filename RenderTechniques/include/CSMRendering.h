#pragma once
#include <PipelineState.h>
#include <TextureD3D12.h>
#include <Camera.h>
#include <Mesh.h>
#include <Light.h>
#include <SimpleMath.h>

using namespace DirectX;
using namespace EduEngine::EduBinding;

namespace EduEngine
{
	class CSMRendering
	{
	public:
		// It should be always 4! Otherwise, everything will break
		static const uint32 MAX_CASCADES = 4;

		struct RenderObject
		{
			Mesh* Mesh;
			XMMATRIX World;
		};

		struct Settings
		{
			uint32 CascadesCount = 4;
			float ShadowDistance = 200;
			XMFLOAT2 CSMSizes[MAX_CASCADES]
			{
				{ 2048, 2048 },
				{ 1024, 1024 },
				{ 512,	512	 },
				{ 256,	256	 },
			};
			float CSMSplits[MAX_CASCADES] { 0.15f, 0.35f, 0.65f, 1.0f };
			XMFLOAT2 ShadowBias = { 0.001f, 0.001f };
		};

	public:
		CSMRendering(RenderDeviceD3D12* device, DeviceContext* context, const Settings& settings = {});

		void Update(DeviceContext* context, Camera* camera, Light* light);
		void Render(DeviceContext* context, RenderObject* objects, uint32 objectsNum);

		int GetCascadeCount() const { return m_Settings.CascadesCount; }
		XMMATRIX GetCascadeTransform(int index) const { return m_CascadeTransforms[index]; }
		float GetCascadeDistance(int index) const { return m_Settings.CSMSplits[index] * m_Settings.ShadowDistance; }
		XMFLOAT4 GetCascadeRad2() const { return m_CascadeSphereRad2; }
		XMFLOAT4 GetCascadeBoundingSphere(int index) const { return m_CascadeSpheres[index]; }

		ResourceHeapView* GetSrv(int index) const { return m_ShadowMaps[index]->GetSRVView(); }

	private:
		XMMATRIX CalculateLightView(Light* light);
		XMMATRIX CalculateCascadeProjection(XMMATRIX lightView, XMFLOAT4 boundingSphere, XMFLOAT2 csmSize);

	private:
		RenderDeviceD3D12* m_Device;
		Settings m_Settings;

		PipelineState m_Pso;
		std::shared_ptr<ShaderBinder> m_Binder;

		std::shared_ptr<DynamicUploadBuffer> m_ObjBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;

		std::unique_ptr<TextureD3D12> m_ShadowMaps[MAX_CASCADES];

		XMFLOAT3 m_LightDirection = {};
		XMFLOAT4X4 m_ViewProj[MAX_CASCADES] = {};
		XMMATRIX m_CascadeTransforms[MAX_CASCADES] = {};
		XMFLOAT4 m_CascadeSpheres[MAX_CASCADES] = {};
		XMFLOAT4 m_CascadeSphereRad2 = {};
	};
}