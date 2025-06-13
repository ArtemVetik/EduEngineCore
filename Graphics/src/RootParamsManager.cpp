#include "RootParamsManager.h"

namespace EduEngine
{
    RootParamsManager::RootParamsManager() :
        m_pMemory(nullptr)
    {
    }

    RootParamsManager::~RootParamsManager()
    {
        FreeMemory();
    }

    void RootParamsManager::AddRootView(D3D12_ROOT_PARAMETER_TYPE parameterType, uint32 rootIndex, UINT sRegister, D3D12_SHADER_VISIBILITY visibility, SHADER_VARIABLE_TYPE varType)
    {
        auto* pRangePtr = Extend(0, 1, 0);
        VERIFY_EXPR((char*)pRangePtr == (char*)m_pMemory + GetRequiredMemorySize(0, 0, 0), "");
        new(m_pRootViews + m_NumRootViews - 1) RootParameter(parameterType, rootIndex, sRegister, 0u, visibility, varType);
    }

    void RootParamsManager::AddRootTable(uint32 rootIndex, D3D12_SHADER_VISIBILITY visibility, SHADER_VARIABLE_TYPE varType, uint32 numRangesInNewTable)
    {
        auto* pRangePtr = Extend(1, 0, numRangesInNewTable);
        VERIFY_EXPR((char*)(pRangePtr + numRangesInNewTable) == (char*)m_pMemory + GetRequiredMemorySize(0, 0, 0), "");
        new(m_pRootTables + m_NumRootTables - 1) RootParameter(D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, rootIndex, numRangesInNewTable, pRangePtr, visibility, varType);
    }

    void RootParamsManager::AddDescriptorRanges(uint32 rootTableInd, uint32 numExtraRanges)
    {
        auto* pRangePtr = Extend(0, 0, numExtraRanges, rootTableInd);
        VERIFY_EXPR((char*)pRangePtr == (char*)m_pMemory + GetRequiredMemorySize(0, 0, 0), "");
    }

    void RootParamsManager::FreeMemory()
    {
        auto* prams = reinterpret_cast<RootParameter*>(m_pMemory);

        for (uint32 i = 0; i < m_NumRootTables + m_NumRootViews; i++)
            prams[i].~RootParameter();

        std::free(m_pMemory);
    }

    size_t RootParamsManager::GetRequiredMemorySize(uint32 numExtraRootTables, uint32 numExtraRootViews, uint32 numExtraDescriptorRanges) const
    {
        return sizeof(RootParameter) * (m_NumRootTables + numExtraRootTables + m_NumRootViews + numExtraRootViews) + sizeof(D3D12_DESCRIPTOR_RANGE) * (m_TotalDescriptorRanges + numExtraDescriptorRanges);
    }

    D3D12_DESCRIPTOR_RANGE* RootParamsManager::Extend(uint32 numExtraRootTables, uint32 numExtraRootViews, uint32 numExtraDescriptorRanges, uint32 rootTableToAddRanges)
    {
        VERIFY_EXPR(numExtraRootTables > 0 || numExtraRootViews > 0 || numExtraDescriptorRanges > 0, "At least one root table, root view or descriptor range must be added");
        auto memorySize = GetRequiredMemorySize(numExtraRootTables, numExtraRootViews, numExtraDescriptorRanges);
        VERIFY_EXPR(memorySize > 0, "");
        void* pNewMemory = std::malloc(memorySize);

        if (pNewMemory == NULL)
            ASSERT_FAILED("failed to allocate memory");

        memset(pNewMemory, 0, memorySize);

        // Note: this order is more efficient than views->tables->ranges
        auto* pNewRootTables = reinterpret_cast<RootParameter*>(pNewMemory);
        auto* pNewRootViews = pNewRootTables + (m_NumRootTables + numExtraRootTables);
        auto* pCurrDescriptorRangePtr = reinterpret_cast<D3D12_DESCRIPTOR_RANGE*>(pNewRootViews + m_NumRootViews + numExtraRootViews);

        // Copy existing root tables to new memory
        for (uint32 rt = 0; rt < m_NumRootTables; ++rt)
        {
            const auto& srcTbl = GetRootTable(rt);
            auto& d3d12SrcTbl = static_cast<const D3D12_ROOT_PARAMETER&>(srcTbl).DescriptorTable;
            auto numRanges = d3d12SrcTbl.NumDescriptorRanges;
            if (rt == rootTableToAddRanges)
            {
                VERIFY_EXPR(numExtraRootTables == 0 || numExtraRootTables == 1, "Up to one descriptor table can be extended at a time");
                numRanges += numExtraDescriptorRanges;
            }
            new(pNewRootTables + rt) RootParameter(srcTbl, numRanges, pCurrDescriptorRangePtr);
            pCurrDescriptorRangePtr += numRanges;
        }

        // Copy existing root view to new memory
        for (uint32 rv = 0; rv < m_NumRootViews; ++rv)
        {
            const auto& SrcView = GetRootView(rv);
            new(pNewRootViews + rv) RootParameter(SrcView);
        }

        if (m_pMemory)
            FreeMemory();

        m_pMemory = pNewMemory;
        m_NumRootTables += numExtraRootTables;
        m_NumRootViews += numExtraRootViews;
        m_TotalDescriptorRanges += numExtraDescriptorRanges;
        m_pRootTables = m_NumRootTables != 0 ? pNewRootTables : nullptr;
        m_pRootViews = m_NumRootViews != 0 ? pNewRootViews : nullptr;

        return pCurrDescriptorRangePtr;
    }

#pragma region RootParameter

    RootParameter::RootParameter(D3D12_ROOT_PARAMETER_TYPE parameterType, uint32 rootIndex, UINT sRegister, UINT registerSpace, D3D12_SHADER_VISIBILITY visibility, SHADER_VARIABLE_TYPE varType) :
        m_RootIndex(rootIndex),
        m_ShaderVarType(varType)
    {
        VERIFY_EXPR(parameterType == D3D12_ROOT_PARAMETER_TYPE_CBV || parameterType == D3D12_ROOT_PARAMETER_TYPE_SRV || parameterType == D3D12_ROOT_PARAMETER_TYPE_UAV, "Unexpected parameter type - verify argument list");
        m_RootParam.ParameterType = parameterType;
        m_RootParam.ShaderVisibility = visibility;
        m_RootParam.Descriptor.ShaderRegister = sRegister;
        m_RootParam.Descriptor.RegisterSpace = registerSpace;
    }

    RootParameter::RootParameter(D3D12_ROOT_PARAMETER_TYPE parameterType, uint32 rootIndex, UINT numRanges, D3D12_DESCRIPTOR_RANGE* ranges, D3D12_SHADER_VISIBILITY visibility, SHADER_VARIABLE_TYPE varType) :
        m_RootIndex(rootIndex),
        m_ShaderVarType(varType)
    {
        VERIFY_EXPR(parameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, "Unexpected parameter type - verify argument list");
        VERIFY_EXPR(ranges != nullptr, "pRanges is null");
        m_RootParam.ParameterType = parameterType;
        m_RootParam.ShaderVisibility = visibility;
        m_RootParam.DescriptorTable.NumDescriptorRanges = numRanges;
        m_RootParam.DescriptorTable.pDescriptorRanges = ranges;

#ifdef _DEBUG
        for (uint32 r = 0; r < numRanges; ++r)
            ranges[r].RangeType = static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(-1);
#endif
    }

    RootParameter::RootParameter(const RootParameter& rp) :
        m_RootParam(rp.m_RootParam),
        m_DescriptorTableSize(rp.m_DescriptorTableSize),
        m_ShaderVarType(rp.m_ShaderVarType),
        m_RootIndex(rp.m_RootIndex)
    {
        VERIFY_EXPR(m_RootParam.ParameterType != D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, "Use another constructor to copy descriptor table");
    }

    RootParameter::RootParameter(const RootParameter& rp, UINT numRanges, D3D12_DESCRIPTOR_RANGE* ranges) :
        m_RootParam(rp.m_RootParam),
        m_DescriptorTableSize(rp.m_DescriptorTableSize),
        m_ShaderVarType(rp.m_ShaderVarType),
        m_RootIndex(rp.m_RootIndex)
    {
        VERIFY_EXPR(m_RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, "Root parameter is expected to be a descriptor table");
        VERIFY_EXPR(numRanges >= m_RootParam.DescriptorTable.NumDescriptorRanges, "New table must be larger than source one");
        auto& dstTbl = m_RootParam.DescriptorTable;
        dstTbl.NumDescriptorRanges = numRanges;
        dstTbl.pDescriptorRanges = ranges;
        const auto& srcTbl = rp.m_RootParam.DescriptorTable;
        memcpy(ranges, srcTbl.pDescriptorRanges, srcTbl.NumDescriptorRanges * sizeof(D3D12_DESCRIPTOR_RANGE));

#ifdef _DEBUG
        {
            uint32 dbgTableSize = 0;
            for (uint32 r = 0; r < srcTbl.NumDescriptorRanges; ++r)
            {
                const auto& Range = srcTbl.pDescriptorRanges[r];
                dbgTableSize = std::max(dbgTableSize, Range.OffsetInDescriptorsFromTableStart + Range.NumDescriptors);
            }
            VERIFY_EXPR(dbgTableSize == m_DescriptorTableSize, "Incorrect descriptor table size");

            for (uint32 r = srcTbl.NumDescriptorRanges; r < dstTbl.NumDescriptorRanges; ++r)
                ranges[r].RangeType = static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(-1);
        }
#endif
    }

    void RootParameter::SetDescriptorRange(UINT rangeIndex, D3D12_DESCRIPTOR_RANGE_TYPE type, UINT sRegister, UINT count, UINT space, UINT offsetFromTableStart)
    {
        VERIFY_EXPR(m_RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, "Incorrect parameter table: descriptor table is expected");
        auto& tbl = m_RootParam.DescriptorTable;
        VERIFY_EXPR(rangeIndex < tbl.NumDescriptorRanges, "Invalid descriptor range index");
        D3D12_DESCRIPTOR_RANGE& range = const_cast<D3D12_DESCRIPTOR_RANGE&>(tbl.pDescriptorRanges[rangeIndex]);
        VERIFY_EXPR(range.RangeType == static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(-1), "Descriptor range has already been initialized. m_DescriptorTableSize may be updated incorrectly");
        range.RangeType = type;
        range.NumDescriptors = count;
        range.BaseShaderRegister = sRegister;
        range.RegisterSpace = space;
        range.OffsetInDescriptorsFromTableStart = offsetFromTableStart;
        m_DescriptorTableSize = std::max(m_DescriptorTableSize, offsetFromTableStart + count);
    }

#pragma endregion
}