#pragma once

#include <memory>

#include "../../BaseDemo/include/RenderEngine.h"
#include "../../BaseDemo/include/RenderPasses.h"
#include "../../Graphics/include/DynamicUploadBuffer.h"

#include <Mesh.h>
#include <Camera.h>
#include <VirtualShadowMap.h>
#include <GBuffer.h>

namespace EduEngine
{
	class VirtualShadowMapsDemo : public RenderEngine
	{
	protected:
		void OnStartUp() override;
		void OnUpdate(const Timer& timer) override;
		void OnRender(const Timer& timer) override;
		void OnResize() override;

	private:
		void RenderImGui();
		void UpdateLightDirection();

	private:
		std::shared_ptr<Mesh> m_Mesh;

		PipelineState m_GeometryPso;
		std::shared_ptr<ShaderBinder> m_GeometryBinder;

		PipelineState m_LightPso;
		std::shared_ptr<ShaderBinder> m_LightBinder;

		PipelineState m_PostProcPso;
		std::shared_ptr<ShaderBinder> m_PostProcBinder;

		std::shared_ptr<DynamicUploadBuffer> m_ObjBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;
		std::shared_ptr<BufferD3D12> m_ConstantsBuffer;

		std::unique_ptr<VirtualShadowMap> m_VSM;
		std::unique_ptr<GBuffer> m_GBuffer;

		DescriptorHeapAllocation m_GpuSSDepth;

		Light m_Light;
		float m_SunAzimuthDegrees = 45.0f;
		float m_SunElevationDegrees = 70.5f;
		float m_AmbientStrength = 0.3f;

		RenderObject m_RenderObjects[1];
	};
}