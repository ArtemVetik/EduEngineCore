#include "DeviceContext.h"

#include <cassert>

namespace EduEngine
{
	DeviceContext::DeviceContext(RenderDeviceD3D12& device, D3D12_COMMAND_LIST_TYPE type)
	{
		assert(type == D3D12_COMMAND_LIST_TYPE_DIRECT || type == D3D12_COMMAND_LIST_TYPE_COMPUTE);

		m_CmdCtx = new CommandContext(device, type);
	}

	DeviceContext::~DeviceContext()
	{
		delete m_CmdCtx;
	}
}