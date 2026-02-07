#pragma once
#include "framework.h"

#include <CSMRendering.h>
#include <SimpleMath.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	class RENDERTECHNIQUES_API ReflectionProbe
	{
	public:
		ReflectionProbe(RenderDeviceD3D12* device, DeviceContext* context);

		void Render(DeviceContext* context, RenderObject* renderObjects, uint32 objectsNum);

	private:
		PipelineState m_PSO;
		std::shared_ptr<ShaderBinder> m_Binder;

		std::shared_ptr<TextureD3D12> m_ReflectionCube;
		std::shared_ptr<TextureD3D12> m_DepthBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_ObjBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;

		XMFLOAT3 m_Center;
		XMFLOAT2 m_BoxSize;
	};
}