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
#include <VirtualShadowMapDefinitions.h>
#include <array>

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
		UINT StatsBuffer;
	};

	enum class VSMDebugMode : uint32 // TODO: Move to PSOEntry
	{
		None = 0,
		PageGrid = 1,
		CacheState = 2,
	};

	inline static const char* VSMDebugModeStr[]
	{
		"None", "PageGrid", "CacheState"
	};

	class VirtualShadowMap
	{
	public:
		VirtualShadowMap(RenderDeviceD3D12* device, DeviceContext* context);

		void SetSSDepth(RenderDeviceD3D12* device, DeviceContext* context, UINT ssDepthTex, XMUINT2 screenSize);
		void BuildCommandSignature(RenderDeviceD3D12* device, DeviceContext* context, RenderObject* objects, uint32 objectsNum);
		
		void SetDebugMode(VSMDebugMode mode) { m_DebugMode = mode; }
		VSMDebugMode GetDebugMode() const { return m_DebugMode; }

#if VSM_GENERATE_STATS
		const std::array<uint32, VSM_STAT_NUM>& GetStats() const { return m_StatsData; }
#endif

		void Update(DeviceContext* context, Camera* camera, Light* light);
		void Compute(DeviceContext* context, RenderObject* objects, uint32 objectsNum);

		TextureD3D12* GetShadowMask() const { return m_ShadowMask.get(); }
		TextureD3D12* GetDebugMask() const { return m_DebugMask.get(); }

	private:
#if VSM_GENERATE_STATS
		static constexpr uint32 STATS_RING = 8;
#endif

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
#if VSM_GENERATE_STATS
		CSPso m_ValidatePso;
#endif

		PipelineState m_ClearSlotsPso;
		std::shared_ptr<ShaderBinder> m_ClearSlotsBinder;

		PipelineState m_GeometryPso;
		std::shared_ptr<ShaderBinder> m_GeometryBinder;

		PipelineState m_DrawShadowPso;
		std::shared_ptr<ShaderBinder> m_DrawShadowBinder;

		PipelineState m_DrawDebugPso;
		std::shared_ptr<ShaderBinder> m_DrawDebugBinder;

		std::shared_ptr<TextureD3D12> m_PhysPool;
		std::shared_ptr<BufferD3D12> m_PhysMeta;
		std::shared_ptr<TextureD3D12> m_PageRequest;
		std::shared_ptr<TextureD3D12> m_PageTable;
		std::shared_ptr<BufferD3D12> m_PageLists[2];
		std::shared_ptr<BufferD3D12> m_RenderList;
		std::shared_ptr<BufferD3D12> m_RenderCount;
#if VSM_GENERATE_STATS
		std::shared_ptr<BufferD3D12> m_StatsBuffer;
		std::unique_ptr<ReadBackBufferD3D12> m_StatsReadbackBuffer;
#endif

		std::shared_ptr<BufferD3D12> m_QuadDrawArgs;
		std::shared_ptr<BufferD3D12> m_DrawArgs;
		std::shared_ptr<TextureD3D12> m_ShadowMask;
		std::shared_ptr<TextureD3D12> m_DebugMask;
		
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
		VSMDebugMode m_DebugMode;
#if VSM_GENERATE_STATS
		std::array<uint32, VSM_STAT_NUM> m_StatsData = {};
#endif
	};
}