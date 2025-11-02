#pragma once
#include "framework.h"
#include "Asserts.h"
#include "ShaderAPI.h"

#include <d3d12.h>

namespace EduEngine
{
	class RootParameter
	{
	public:

		// Create CbvSrvUav only
		RootParameter(D3D12_ROOT_PARAMETER_TYPE parameterType, uint32 rootIndex, UINT sRegister, UINT registerSpace, D3D12_SHADER_VISIBILITY visibility, SHADER_VARIABLE_TYPE varType);

		// Create DescriptorTable only
		RootParameter(D3D12_ROOT_PARAMETER_TYPE parameterType, uint32 rootIndex, UINT numRanges, D3D12_DESCRIPTOR_RANGE* ranges, D3D12_SHADER_VISIBILITY visibility, SHADER_VARIABLE_TYPE varType);

		// Copy for CbvSrvUav only
		RootParameter(const RootParameter& rp);

		// Copy for DescriptorTable only
		RootParameter(const RootParameter& rp, UINT numRanges, D3D12_DESCRIPTOR_RANGE* ranges);

		RootParameter& operator = (const RootParameter& rp) = delete;
		RootParameter& operator = (RootParameter&& rp) = delete;

		void SetDescriptorRange(UINT rangeIndex, D3D12_DESCRIPTOR_RANGE_TYPE type, UINT sRegister, UINT count, UINT space = 0, UINT offsetFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);

		uint32 GetDescriptorTableSize() const
		{
			VERIFY_EXPR(m_RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, "Incorrect parameter table: descriptor table is expected");
			return m_DescriptorTableSize;
		}

		SHADER_VARIABLE_TYPE GetShaderVariableType() const { return m_ShaderVarType; }
		uint32 GetRootIndex() const { return m_RootIndex; }
		operator const D3D12_ROOT_PARAMETER& () const { return m_RootParam; }

	private:
		SHADER_VARIABLE_TYPE m_ShaderVarType = static_cast<SHADER_VARIABLE_TYPE>(-1);
		D3D12_ROOT_PARAMETER m_RootParam = {};
		uint32 m_DescriptorTableSize = 0;
		uint32 m_RootIndex = static_cast<uint32>(-1);
	};

	// -------------- Root Tables ---------------------- Root Views --------------------------- Descriptor Ranges ----------------
	// |                                   |                                   |                                                 |
	// |                                   |                                   |                                                 |
	// RootTables[0] | ... | RootTables[n] | RootViews[0] | ... | RootViews[m] | DescriptorRanges[0] | ... | DescriptorRanges[k] |
	//
	class RootParamsManager
	{
	public:
		RootParamsManager();
		~RootParamsManager();

		RootParamsManager(const RootParamsManager&) = delete;
		RootParamsManager& operator= (const RootParamsManager&) = delete;
		RootParamsManager(RootParamsManager&&) = delete;
		RootParamsManager& operator= (RootParamsManager&&) = delete;

		uint32 GetNumRootTables() const { return m_NumRootTables; }
		uint32 GetNumRootViews() const { return m_NumRootViews; }

		const RootParameter& GetRootTable(uint32 tableInd) const
		{
			VERIFY_EXPR(tableInd < m_NumRootTables, "");
			return m_pRootTables[tableInd];
		}

		RootParameter& GetRootTable(uint32 tableInd)
		{
			VERIFY_EXPR(tableInd < m_NumRootTables, "");
			return m_pRootTables[tableInd];
		}

		const RootParameter& GetRootView(uint32 viewInd)const
		{
			VERIFY_EXPR(viewInd < m_NumRootViews, "");
			return m_pRootViews[viewInd];
		}

		RootParameter& GetRootView(uint32 viewInd)
		{
			VERIFY_EXPR(viewInd < m_NumRootViews, "");
			return m_pRootViews[viewInd];
		}

		void AddRootView(D3D12_ROOT_PARAMETER_TYPE parameterType, uint32 rootIndex, UINT sRegister, D3D12_SHADER_VISIBILITY visibility, SHADER_VARIABLE_TYPE varType);
		void AddRootTable(uint32 rootIndex, D3D12_SHADER_VISIBILITY visibility, SHADER_VARIABLE_TYPE varType, uint32 numRangesInNewTable = 1);
		void AddDescriptorRanges(uint32 rootTableInd, uint32 numExtraRanges = 1);

		template<class TOperation>
		void ProcessRootTables(TOperation)const;

	private:
		void FreeMemory();

		size_t GetRequiredMemorySize(uint32 numExtraRootTables, uint32 numExtraRootViews, uint32 numExtraDescriptorRanges)const;
		D3D12_DESCRIPTOR_RANGE* Extend(uint32 numExtraRootTables, uint32 numExtraRootViews, uint32 numExtraDescriptorRanges, uint32 rootTableToAddRanges = static_cast<uint32>(-1));

		void* m_pMemory;
		uint32 m_NumRootTables = 0;
		uint32 m_NumRootViews = 0;
		uint32 m_TotalDescriptorRanges = 0;
		RootParameter* m_pRootTables = nullptr;
		RootParameter* m_pRootViews = nullptr;
	};

	template<class TOperation>
	inline void RootParamsManager::ProcessRootTables(TOperation Operation) const
	{
		for (uint32 rt = 0; rt < m_NumRootTables; ++rt)
		{
			auto& rootTable = GetRootTable(rt);
			auto rootInd = rootTable.GetRootIndex();
			const D3D12_ROOT_PARAMETER& d3d12Param = rootTable;

			VERIFY_EXPR(d3d12Param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, "");

			auto& d3d12Table = d3d12Param.DescriptorTable;
			VERIFY_EXPR(d3d12Table.NumDescriptorRanges > 0 && rootTable.GetDescriptorTableSize() > 0, "Unexepected empty descriptor table");
			bool isResourceTable = d3d12Table.pDescriptorRanges[0].RangeType != D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;

			Operation(rootInd, rootTable, d3d12Param, isResourceTable);
		}
	}
}