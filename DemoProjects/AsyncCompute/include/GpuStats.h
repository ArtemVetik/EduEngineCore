#pragma once
#include <RenderDeviceD3D12.h>
#include <BufferD3D12.h>
#include <memory>
#include <array>

namespace EduEngine
{
	class GpuStats
	{
	public:
		GpuStats(RenderDeviceD3D12* device);

		void MarkStartComputeWork(DeviceContext* context);
		void MarkEndComputeWork(DeviceContext* context);
		void MarkStartDrawWork(DeviceContext* context);
		void MarkEndDrawWork(DeviceContext* context);

		void Update(float deltaTime);

		void DrawImGui(bool asyncCompute);

	private:
		static constexpr uint8 BuffBits = 3;
		static constexpr float UpdateTime = 0.2f;
		
		struct FrameStats
		{
			UINT64 ComputeStart = 0;
			UINT64 ComputeEnd = 0;
			UINT64 DrawStart = 0;
			UINT64 DrawEnd = 0;
		};
		
		struct FrameRenderStats
		{
			float ComputeStart = 0.0f;
			float ComputeEnd = 0.0f;
			UINT64 ComputeDelta = 0;
			float DrawStart = 0.0f;
			float DrawEnd = 0.0f;
			UINT64 DrawDelta = 0;
			UINT64 TotalDelta = 0;
		};
		
		const QueryHeap& m_QueryHeap;

		std::unique_ptr<ReadBackBufferD3D12> m_ComputeReadBackBuffer;
		std::unique_ptr<ReadBackBufferD3D12> m_DrawReadBackBuffer;
		UINT64 m_DirectFrequency;
		UINT64 m_ComputeFrequency;

		uint64 m_FrameNum : BuffBits;
		std::array<FrameStats, (1 << BuffBits)> m_FramesBuffer;
		FrameRenderStats m_RenderStats;
		float m_AccumTime;
	};
}