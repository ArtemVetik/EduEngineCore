#pragma once
#include "framework.h"
#include "CommandContext.h"
#include "QueueID.h"

namespace EduEngine
{
	class GRAPHICS_API DeviceContext
	{
	public:
		DeviceContext(RenderDeviceD3D12& device, D3D12_COMMAND_LIST_TYPE type);
		~DeviceContext();

		CommandContext* GetCommandCtx() const { return m_CmdCtx; }

	private:
		CommandContext* m_CmdCtx;
	};
}