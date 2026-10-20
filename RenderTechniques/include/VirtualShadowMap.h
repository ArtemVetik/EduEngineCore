#pragma once
#include "framework.h"

#include <PipelineState.h>
#include <TextureD3D12.h>
#include <BufferD3D12.h>
#include <ComputePipelineState.h>
#include <DynamicUploadBuffer.h>
#include <Camera.h>
#include <Light.h>
#include <CommandSignatureD3D12.h>
#include <RenderObject.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) CbBuffers
	{
		UINT PhysMeta;
		UINT PageRequest;
		UINT PageTable;
		UINT RenderList;
		UINT RenderCount;
		UINT SSDepthTexture;
		UINT QuadDrawArgs;
		UINT DrawArgs;
		UINT DrawCount;
		UINT PhysPool;
	};

	class VirtualShadowMap
	{
	public:
		VirtualShadowMap(RenderDeviceD3D12* device, DeviceContext* context);

		void SetSSDepth(RenderDeviceD3D12* device, DeviceContext* context, UINT ssDepthTex, XMUINT2 screenSize);
		void BuildCommandSignature(RenderDeviceD3D12* device, DeviceContext* context, RenderObject* objects, uint32 objectsNum);

		void Update(DeviceContext* context, Camera* camera, Light* light);
		void Compute(DeviceContext* context, RenderObject* objects, uint32 objectsNum);

		TextureD3D12* GetShadowMask() const { return m_ShadowMask.get(); }

	private:
		static constexpr uint32 PAGE_SIZE = 128;
		static constexpr uint32 PAGES_XY = 128;
		static constexpr uint32 POOL_SIZE = 64;

	private:
		struct CSPso
		{
			ComputePipelineState PSO;
			std::shared_ptr<ShaderBinder> Binder;

			CSPso(QueueId queueId = QueueId::Direct) :
				PSO(queueId)
			{ }
		};

		CSPso m_ClearPso;
		CSPso m_RemapPso;
		CSPso m_MarkPso;
		CSPso m_UpdatePso;
		CSPso m_PackPso;
		CSPso m_AllocatePso;
		CSPso m_FinalizePso;
		CSPso m_DrawArgsPso;

		PipelineState m_ClearSlotsPso;
		std::shared_ptr<ShaderBinder> m_ClearSlotsBinder;

		PipelineState m_GeometryPso;
		std::shared_ptr<ShaderBinder> m_GeometryBinder;

		PipelineState m_DrawShadowPso;
		std::shared_ptr<ShaderBinder> m_DrawShadowBinder;

		std::shared_ptr<TextureD3D12> m_PhysPool;
		std::shared_ptr<BufferD3D12> m_PhysMeta;
		std::shared_ptr<TextureD3D12> m_PageRequest;
		std::shared_ptr<TextureD3D12> m_PageTable;
		std::shared_ptr<BufferD3D12> m_PageLists[2];
		std::shared_ptr<BufferD3D12> m_RenderList;
		std::shared_ptr<BufferD3D12> m_RenderCount;

		std::shared_ptr<BufferD3D12> m_QuadDrawArgs;
		std::shared_ptr<BufferD3D12> m_DrawArgs;
		std::shared_ptr<TextureD3D12> m_ShadowMask;
		
		std::shared_ptr<BufferD3D12> m_ConstantsBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_ObjBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;

		CommandSignatureD3D12 m_QuadDrawArgsSignature;
		CommandSignatureD3D12 m_DrawArgsSignature;

		CbBuffers m_CbBuffers;
		XMUINT2 m_ScreenSize;
		XMINT2 m_PrevPage;
		XMFLOAT3 m_PrevLightDir;
		UINT m_Frame;
		UINT m_CurListIdx;
		UINT m_PrevListIdx;
	};
}