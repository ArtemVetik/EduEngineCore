#pragma once
#include "framework.h"
#include "QueueMask.h"

namespace EduEngine
{
    class GRAPHICS_HEAPS_API ReleaseResourceWrapper;

    class GRAPHICS_HEAPS_API IRenderDeviceD3D12
    {
    public:
        virtual ID3D12Device* GetD3D12Device() const = 0;
        virtual void SafeReleaseObject(QueueMask queueMask, ReleaseResourceWrapper&& wrapper) = 0;
    };
}