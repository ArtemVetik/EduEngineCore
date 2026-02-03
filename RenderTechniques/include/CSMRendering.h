#pragma once
#include <PipelineState.h>
#include <TextureD3D12.h>
#include <Camera.h>
#include <Mesh.h>
#include <SimpleMath.h>

using namespace DirectX;
using namespace EduEngine::EduBinding;

namespace EduEngine
{
	class CSMRendering
	{
	public:
		struct Light
		{
			XMFLOAT3 Position;
			XMFLOAT3 Direction;
		};

	public:
		CSMRendering(RenderDeviceD3D12* device, DeviceContext* context);

		void Render(DeviceContext* context, Camera* camera, Light* light, const Mesh* mesh); // TODO: replace single mesh with list of render objects

		int GetCascadeCount() const { return m_CascadeCount; }
		XMMATRIX GetCascadeTransform(int index) const { return m_CascadeTransforms[index]; }
		float GetCascadeDistance(int index) const { return m_CascadeSplits[index] * ShadowDistance; }
		D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle() const { return m_ShadowMaps[0]->GetSRVView()->GetGpuHandle(); }

		static constexpr int MaxCascades = 4;
		static constexpr float ShadowDistance = 150.0f;

		static constexpr XMFLOAT2 CSMSizes[4] =
		{
			{ 2048, 2048 },
			{ 1024, 1024 },
			{ 512,	512	 },
			{ 256,	256	 },
		};
		static constexpr float CSMSplits[4] = { 0.25f, 0.50f, 0.75f, 1.0f };

	private:
		XMMATRIX CalculateLightView(Light* light);
		XMMATRIX CalculateCascadeProjection(float nearDist, float farDist, Camera* camera, XMMATRIX lightView);

	private:
		RenderDeviceD3D12* m_Device;

		PipelineState m_Pso;
		std::shared_ptr<ShaderBinder> m_Binder;

		std::shared_ptr<DynamicUploadBuffer> m_ObjBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;

		int m_CascadeCount;
		float m_CascadeSplits[MaxCascades];
		XMMATRIX m_CascadeTransforms[MaxCascades];
		std::unique_ptr<TextureD3D12> m_ShadowMaps[MaxCascades];
	};
}