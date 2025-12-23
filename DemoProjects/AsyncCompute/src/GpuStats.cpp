#include "GpuStats.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

namespace EduEngine
{
	GpuStats::GpuStats(RenderDeviceD3D12* device) :
		m_QueryHeap(device->GetQueryHeap()),
		m_FrameNum(0),
		m_FramesBuffer{},
		m_RenderStats{},
		m_AccumTime(0.0f)
	{
		m_ComputeReadBackBuffer = std::make_unique<ReadBackBufferD3D12>(device, 16, QueueId::Direct | QueueId::Compute);
		m_ComputeReadBackBuffer->SetName(L"m_ComputeReadBackBuffer");

		m_DrawReadBackBuffer = std::make_unique<ReadBackBufferD3D12>(device, 16, QueueId::Direct);
		m_DrawReadBackBuffer->SetName(L"m_DrawReadBackBuffer");

		device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT).GetD3D12CommandQueue()->GetTimestampFrequency(&m_DirectFrequency);
		device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE).GetD3D12CommandQueue()->GetTimestampFrequency(&m_ComputeFrequency);
	}

	void GpuStats::MarkStartComputeWork(DeviceContext* context)
	{
		m_QueryHeap.EndQuery(*context->GetCommandCtx(), D3D12_QUERY_TYPE_TIMESTAMP, m_FrameNum * 4 + 0);
	}

	void GpuStats::MarkEndComputeWork(DeviceContext* context)
	{
		m_QueryHeap.EndQuery(*context->GetCommandCtx(), D3D12_QUERY_TYPE_TIMESTAMP, m_FrameNum * 4 + 1);
		m_QueryHeap.ResolveQueryData(*context->GetCommandCtx(), D3D12_QUERY_TYPE_TIMESTAMP, m_FrameNum * 4 + 0, 2, m_ComputeReadBackBuffer.get(), sizeof(UINT64) * m_FrameNum * 2);
	}

	void GpuStats::MarkStartDrawWork(DeviceContext* context)
	{
		m_QueryHeap.EndQuery(*context->GetCommandCtx(), D3D12_QUERY_TYPE_TIMESTAMP, m_FrameNum * 4 + 2);
	}

	void GpuStats::MarkEndDrawWork(DeviceContext* context)
	{
		m_QueryHeap.EndQuery(*context->GetCommandCtx(), D3D12_QUERY_TYPE_TIMESTAMP, m_FrameNum * 4 + 3);
		m_QueryHeap.ResolveQueryData(*context->GetCommandCtx(), D3D12_QUERY_TYPE_TIMESTAMP, m_FrameNum * 4 + 2, 2, m_DrawReadBackBuffer.get(), sizeof(UINT64) * m_FrameNum * 2);
	}

	void GpuStats::Update(float deltaTime)
	{
		FrameStats& frame = m_FramesBuffer[m_FrameNum];

		m_ComputeReadBackBuffer->ReadData(&frame.ComputeStart, sizeof(UINT64) * 2, sizeof(UINT64) * m_FrameNum * 2);
		m_DrawReadBackBuffer->ReadData(&frame.DrawStart, sizeof(UINT64) * 2, sizeof(UINT64) * m_FrameNum * 2);

		m_FrameNum++;
		m_AccumTime += deltaTime;

		if (m_AccumTime > UpdateTime)
		{
			for (size_t i = 0; i < m_FramesBuffer.size(); i++)
			{
				FrameStats& frame = m_FramesBuffer[i];

				UINT64 frameStart = std::min(frame.ComputeStart, frame.DrawStart);
				UINT64 frameEnd = std::max(frame.ComputeEnd, frame.DrawEnd);

				UINT64 computeDelta = frame.ComputeEnd - frame.ComputeStart;
				UINT64 drawDelta = frame.DrawEnd - frame.DrawStart;
				UINT64 frameDelta = frameEnd - frameStart;

				if (frameDelta == 0)
					continue;

				m_RenderStats.ComputeStart += (frame.ComputeStart - frameStart) / (float)frameDelta;
				m_RenderStats.ComputeEnd += (frame.ComputeEnd - frameStart) / (float)frameDelta;
				m_RenderStats.DrawStart += (frame.DrawStart - frameStart) / (float)frameDelta;
				m_RenderStats.DrawEnd += (frame.DrawEnd - frameStart) / (float)frameDelta;

				m_RenderStats.ComputeDelta += frame.ComputeEnd - frame.ComputeStart;
				m_RenderStats.DrawDelta += frame.DrawEnd - frame.DrawStart;
				m_RenderStats.TotalDelta += frameEnd - frameStart;
			}

			m_RenderStats.ComputeStart /= m_FramesBuffer.size();
			m_RenderStats.ComputeEnd /= m_FramesBuffer.size();
			m_RenderStats.DrawStart /= m_FramesBuffer.size();
			m_RenderStats.DrawEnd /= m_FramesBuffer.size();

			m_RenderStats.ComputeDelta /= m_FramesBuffer.size();
			m_RenderStats.DrawDelta /= m_FramesBuffer.size();
			m_RenderStats.TotalDelta /= m_FramesBuffer.size();

			m_AccumTime = 0.0f;
		}
	}

	void GpuStats::DrawImGui(bool asyncCompute)
	{
		float currentComputeTime = (m_RenderStats.ComputeDelta / static_cast<double>(asyncCompute ? m_ComputeFrequency : m_DirectFrequency)) * 1000.0;
		float currentDrawTime = (m_RenderStats.DrawDelta / static_cast<double>(m_DirectFrequency)) * 1000.0;
		float currentTotalTime = (m_RenderStats.TotalDelta / static_cast<double>(m_DirectFrequency)) * 1000.0;

		ImGui::SetNextWindowSize(ImVec2(350, 150), ImGuiCond_Always);
		ImGui::Begin("GPU Time", nullptr, ImGuiWindowFlags_NoResize);
		ImDrawList* draw = ImGui::GetWindowDrawList();
		ImVec2 origin = ImGui::GetCursorScreenPos();
		float width = ImGui::GetContentRegionAvail().x;

		const float laneHeight = 20.0f;

		auto RenderLine = [&](int index, float start, float end, const char* text, ImColor color)
			{
				float x0 = origin.x + start * width;
				float x1 = origin.x + end * width;
				float y = origin.y + index * laneHeight;

				draw->AddRectFilled(
					ImVec2(x0, y),
					ImVec2(x1, y + laneHeight - 2),
					color
				);

				draw->AddText(ImVec2(x0 + 3, y + 2), IM_COL32_WHITE, text);
			};

		RenderLine(0, m_RenderStats.DrawStart, m_RenderStats.DrawEnd, "Draw", ImColor(0, 0, 255));
		RenderLine(1, m_RenderStats.ComputeStart, m_RenderStats.ComputeEnd, "Compute", ImColor(255, 165, 0));

		ImVec2 offset(0, laneHeight * 2.0f + 10);
		ImGui::Dummy(offset);
		ImGui::Text("Total GPU Time:	%f", currentTotalTime);
		ImGui::Text("Compute GPU Time:	%f", currentComputeTime);
		ImGui::Text("Draw GPU Time:		%f", currentDrawTime);

		ImGui::End();
	}
}