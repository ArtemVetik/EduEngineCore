#include "ShaderResourceCacheD3D12.h"
#include "DebugEnumPrint.h"

namespace EduEngine::DiligentBinding
{
    ShaderResourceCacheD3D12::~ShaderResourceCacheD3D12()
    {
        if (!m_pMemory)
            return;

        auto* pTables = reinterpret_cast<RootTable*>(m_pMemory);
        auto* pResources = reinterpret_cast<Resource*>(pTables + m_NumTables);
        uint32 resCount = 0;
        
        for (uint32 i = 0; i < m_NumTables; i++)
            resCount += pTables[i].GetNumResources();

        for (uint32 i = 0; i < resCount; i++)
            pResources[i].~Resource();
        
        for (uint32 i = 0; i < m_NumTables; i++)
            pTables[i].~RootTable();

        std::free(m_pMemory);
    }

    void ShaderResourceCacheD3D12::Initialize(uint32 numTables, uint32 tableSizes[])
    {
        // Memory layout:
        //                                         __________________________________________________________
        //                                        |             m_pResources, m_NumResources                 |
        //  m_pMemory                             |                                                          |
        //  |                                     |                                                          V
        //  |  RootTable[0]  |   ....    |  RootTable[Nrt-1]  |  Res[0]  |  ... |  Res[n-1]  |    ....     | Res[0]  |  ... |  Res[m-1]  |
        //       |                                                A  
        //       |                                                |   
        //       |________________________________________________|                 
        //                    m_pResources, m_NumResources                            
        //                                                             

        m_NumTables = numTables;
        uint32 TotalResources = 0;
        for (uint32 t = 0; t < numTables; ++t)
            TotalResources += tableSizes[t];
        auto MemorySize = numTables * sizeof(RootTable) + TotalResources * sizeof(Resource);
        if (MemorySize > 0)
        {
            m_pMemory = std::malloc(MemorySize);
            if (!m_pMemory)
                ASSERT_FAILED("Failed to allocate memory for shader resource cache data");

            auto* pTables = reinterpret_cast<RootTable*>(m_pMemory);
            auto* pCurrResPtr = reinterpret_cast<Resource*>(pTables + m_NumTables);
            for (uint32 res = 0; res < TotalResources; ++res)
                new(pCurrResPtr + res) Resource();

            for (uint32 t = 0; t < numTables; ++t)
            {
                new(&GetRootTable(t)) RootTable(tableSizes[t], tableSizes[t] > 0 ? pCurrResPtr : nullptr);
                pCurrResPtr += tableSizes[t];
            }

            VERIFY_EXPR((char*)pCurrResPtr == (char*)m_pMemory + MemorySize, "");
        }
    }

    void ShaderResourceCacheD3D12::SetDescriptorHeapSpace(DescriptorHeapAllocation&& CbcSrvUavHeapSpace, DescriptorHeapAllocation&& SamplerHeapSpace)
    {
        VERIFY_EXPR(m_SamplerHeapSpace.GetCpuHandle().ptr == 0 && m_CbcSrvUavHeapSpace.GetCpuHandle().ptr == 0, "Space has already been allocated in GPU descriptor heaps");

        m_CbcSrvUavHeapSpace = std::move(CbcSrvUavHeapSpace);
        m_SamplerHeapSpace = std::move(SamplerHeapSpace);
    }

#ifdef _DEBUG
    void ShaderResourceCacheD3D12::DebugPrint()
    {
        printf("-------------------------------------------------\n");
        printf("-------------- ShaderResourceCache --------------\n");
        printf("-------------------------------------------------\n");
        printf("CbcSrvUavHeap Size: %u\n", m_CbcSrvUavHeapSpace.GetDescriptorSize());
        printf("SamplerHeap Size: %u\n", m_SamplerHeapSpace.GetDescriptorSize());
        printf("NumRootTables: %u\n", m_NumTables);

        for (uint32 i = 0; i < m_NumTables; i++)
        {
            auto& t = GetRootTable(i);
            printf("Table %u:: NumResources: %u\t TableStartOffset: %u\n", i, t.GetNumResources(), t.TableStartOffset);
            for (uint32 j = 0; j < t.GetNumResources(); j++)
            {
                printf("\tType: %s\tCPUHandle: %p\tpObject: %p\tpDynObject: %p\n",
                    ResTypeStr(t.GetResource(j).Type), t.GetResource(j).CPUDescriptorHandle, t.GetResource(j).pObject.get(), t.GetResource(j).pDynObject.get());
            }
        }
    }
#endif
}