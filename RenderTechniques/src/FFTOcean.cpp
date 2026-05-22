#include "FFTOcean.h"

#include <cmath>
#include <random>
#include <SimpleMath.h>

using namespace DirectX;

namespace EduEngine
{
	const int LOCAL_WORK_GROUPS_X = 8;
	const int LOCAL_WORK_GROUPS_Y = 8;

	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) InitialSpectrumData
	{
		float WindSpeed;
		float WindDirectionX;
		float WindDirectionY;
		float Gravity;
		float Fetch;
		float Depth;

		UINT RandomNoiseTextureIdx;
		UINT InitialSpectrumTexturesIdx;
		UINT WavesDataTexturesIdx;
		UINT WavelengthsIdx;
		UINT CutoffsIdx;
		UINT FadesIdx;
		UINT SwellsIdx;
	};

	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) ComputeConstantData
	{
		UINT NbCascades;
		UINT TextureSize;
	};

	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) DrawConstantData
	{
		UINT MaxLODLevel;
		UINT TesselationLevel;
		float MaxTesselationDistance;
		float TesselationDecayFactor;
		float CullingTollerance;

		UINT NbCascades;
		UINT WavelengthsIdx;
		UINT DisplacementsTexturesIdx;
		UINT DerivativesTexturesIdx;
		UINT TurbulenceTexturesIdx;
		UINT ReflectionCubeIdx;

		float EnvironmentReflectionStrength;
		XMFLOAT3 SubsurfaceScatteringColor;
		float SubsurfaceScatteringIntensity;
		XMFLOAT3 DeepWaterColor;
		float WaterFogDensity;
		float RefractionStrength;
		float Roughness;
		float AnisoEX;
		float AnisoEY;
		float FoamBlending;
		float FoamThreshold;
		XMFLOAT2 FoamPadRow;
		XMFLOAT3 FoamColor;
		float FoamPad0;
		XMFLOAT3 ShadowsColor;
		float ShadowsIntensity;
		float SunReflectionStrength;
	};

	FFTOcean::FFTOcean(RenderDeviceD3D12* device, DeviceContext* context, InitialSettings initialSettings) :
		m_Context(context),
		m_AtmosphereCube(initialSettings.AtmosphereCube),
		m_CascadesCount(initialSettings.CascadesCount),
		m_TextureSize(initialSettings.TextureSize)
	{
		D3D12_RESOURCE_DESC buffDesc = {};
		buffDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		buffDesc.Width = sizeof(ComputeConstantData);
		buffDesc.Alignment = 0;
		buffDesc.Height = 1;
		buffDesc.DepthOrArraySize = 1;
		buffDesc.MipLevels = 1;
		buffDesc.SampleDesc.Count = 1;
		buffDesc.SampleDesc.Quality = 0;
		buffDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		buffDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		ComputeConstantData computeConstantsData = {};
		computeConstantsData.NbCascades = m_CascadesCount;
		computeConstantsData.TextureSize = m_TextureSize;

		m_ComputeConstantsBuffer = std::make_shared<BufferD3D12>(device, context, buffDesc, QueueId::Direct);
		m_ComputeConstantsBuffer->LoadData(context, &computeConstantsData);

		buffDesc.Width = sizeof(DrawConstantData);
		m_DrawConstantsBuffer = std::make_shared<BufferD3D12>(device, context, buffDesc, QueueId::Direct);

		m_IFFT = std::make_unique<IFFT>(device, context, m_TextureSize, m_CascadesCount, m_ComputeConstantsBuffer);

		GenerateRandomNoiseTexture(device, context);

		auto CreateTexture = [&](uint32 depth, DXGI_FORMAT format, std::shared_ptr<TextureD3D12>& texture, const wchar_t* name, bool createSrv = false)
			{
				D3D12_RESOURCE_DESC texDesc = {};
				texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				texDesc.Alignment = 0;
				texDesc.MipLevels = 1;
				texDesc.DepthOrArraySize = depth;
				texDesc.Width = m_TextureSize;
				texDesc.Height = m_TextureSize;
				texDesc.SampleDesc.Count = 1;
				texDesc.SampleDesc.Quality = 0;
				texDesc.Format = format;
				texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
				texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

				D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
				uavDesc.Format = texDesc.Format;
				uavDesc.Texture2DArray.ArraySize = depth;
				uavDesc.Texture2DArray.FirstArraySlice = 0;
				uavDesc.Texture2DArray.MipSlice = 0;
				uavDesc.Texture2DArray.PlaneSlice = 0;

				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
				srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
				srvDesc.Texture2DArray.ArraySize = depth;
				srvDesc.Texture2DArray.FirstArraySlice = 0;
				srvDesc.Texture2DArray.MipLevels = 1;
				srvDesc.Texture2DArray.MostDetailedMip = 0;
				srvDesc.Texture2DArray.PlaneSlice = 0;
				srvDesc.Texture2DArray.ResourceMinLODClamp = 0;

				texture = std::make_shared<TextureD3D12>(device, texDesc, nullptr, QueueId::Direct);
				texture->CreateUAV(&uavDesc, false);
				if (createSrv)
					texture->CreateSRV(&srvDesc, false);

				texture->SetName(name);
			};

		CreateTexture(m_CascadesCount, DXGI_FORMAT_R32G32B32A32_FLOAT, m_WavesDataTextures, L"WavesDataTextures", true);
		CreateTexture(m_CascadesCount, DXGI_FORMAT_R32G32B32A32_FLOAT, m_InitialSpectrumTextures, L"InitialSpectrumTextures", true);
		CreateTexture(m_CascadesCount, DXGI_FORMAT_R32G32B32A32_FLOAT, m_DisplacementsTextures, L"DisplacementsTextures", true);
		CreateTexture(m_CascadesCount, DXGI_FORMAT_R32G32B32A32_FLOAT, m_DerivativesTextures, L"DerivativesTextures", true);
		CreateTexture(m_CascadesCount, DXGI_FORMAT_R32G32B32A32_FLOAT, m_TurbulenceTextures, L"TurbulenceTextures", true);
		CreateTexture(m_CascadesCount, DXGI_FORMAT_R32G32_FLOAT, m_DxDzTextures, L"DxDzTextures");
		CreateTexture(m_CascadesCount, DXGI_FORMAT_R32G32_FLOAT, m_DyDxzTextures, L"DyDxzTextures");
		CreateTexture(m_CascadesCount, DXGI_FORMAT_R32G32_FLOAT, m_DyxDyzTextures, L"DyxDyzTextures");
		CreateTexture(m_CascadesCount, DXGI_FORMAT_R32G32_FLOAT, m_DxxDzzTextures, L"DxxDzzTextures");

		float wavelengths[MaxCascades];
		float cutoffs[MaxCascades * 2];
		float swells[MaxCascades];
		float fades[MaxCascades];

		for (int i = 0; i < m_CascadesCount; i++) {
			wavelengths[i] = initialSettings.Cascades[i].Wavelength;
			cutoffs[i * 2] = initialSettings.Cascades[i].CutoffLow;
			cutoffs[i * 2 + 1] = initialSettings.Cascades[i].CutoffHigh;
			swells[i] = initialSettings.Cascades[i].Swell;
			fades[i] = initialSettings.Cascades[i].Fade;
		}

		auto CreateBuffer = [&](uint32 size, std::shared_ptr<BufferD3D12>& buffer, void* data, const wchar_t* name)
			{
				D3D12_RESOURCE_DESC buffDesc = {};
				buffDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
				buffDesc.Width = sizeof(float) * size;
				buffDesc.Alignment = 0;
				buffDesc.Height = 1;
				buffDesc.DepthOrArraySize = 1;
				buffDesc.MipLevels = 1;
				buffDesc.SampleDesc.Count = 1;
				buffDesc.SampleDesc.Quality = 0;
				buffDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
				buffDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				srvDesc.Buffer.FirstElement = 0;
				srvDesc.Buffer.NumElements = size;
				srvDesc.Buffer.StructureByteStride = sizeof(float);
				srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

				buffer = std::make_shared<BufferD3D12>(device, context, buffDesc, QueueId::Direct);
				buffer->CreateSRV(&srvDesc, false);
				buffer->SetName(name);

				buffer->LoadData(context, data);
			};

		CreateBuffer(m_CascadesCount, m_Wavelengths, wavelengths, L"Wavelengths");
		CreateBuffer(m_CascadesCount * 2, m_Cutoffs, cutoffs, L"Cutoffs");
		CreateBuffer(m_CascadesCount, m_Swells, swells, L"Swells");
		CreateBuffer(m_CascadesCount, m_Fades, fades, L"Fades");

		buffDesc = {};
		buffDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		buffDesc.Width = sizeof(InitialSpectrumData);
		buffDesc.Alignment = 0;
		buffDesc.Height = 1;
		buffDesc.DepthOrArraySize = 1;
		buffDesc.MipLevels = 1;
		buffDesc.SampleDesc.Count = 1;
		buffDesc.SampleDesc.Quality = 0;
		buffDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		buffDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		m_InitSpectrumBuffer = std::make_shared<BufferD3D12>(device, context, buffDesc, QueueId::Direct);
		m_InitSpectrumBuffer->SetName(L"InitSpectrumBuffer");

		// TimeDependentSpectrum.hlsl
		{
			struct InputTextures
			{
				UINT DxDzTexturesIdx;
				UINT DyDxzTexturesIdx;
				UINT DyxDyzTexturesIdx;
				UINT DxxDzzTexturesIdx;
				UINT ConjugatedInitialSpectrumTexturesIdx;
				UINT WavesDataTexturesIdx;
			} inputTextures;

			inputTextures.DxDzTexturesIdx = m_DxDzTextures->GetUAVView()->GetGpuHeapIndex();
			inputTextures.DyDxzTexturesIdx = m_DyDxzTextures->GetUAVView()->GetGpuHeapIndex();
			inputTextures.DyxDyzTexturesIdx = m_DyxDyzTextures->GetUAVView()->GetGpuHeapIndex();
			inputTextures.DxxDzzTexturesIdx = m_DxxDzzTextures->GetUAVView()->GetGpuHeapIndex();
			inputTextures.ConjugatedInitialSpectrumTexturesIdx = m_InitialSpectrumTextures->GetSRVView()->GetGpuHeapIndex();
			inputTextures.WavesDataTexturesIdx = m_WavesDataTextures->GetSRVView()->GetGpuHeapIndex();

			buffDesc = {};
			buffDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			buffDesc.Width = sizeof(InputTextures);
			buffDesc.Alignment = 0;
			buffDesc.Height = 1;
			buffDesc.DepthOrArraySize = 1;
			buffDesc.MipLevels = 1;
			buffDesc.SampleDesc.Count = 1;
			buffDesc.SampleDesc.Quality = 0;
			buffDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			buffDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

			m_TimeDependentBuffer = std::make_shared<BufferD3D12>(device, context, buffDesc, QueueId::Direct);
			m_TimeDependentBuffer->SetName(L"TimeDependentBuffer");
			m_TimeDependentBuffer->LoadData(context, &inputTextures);
		}

		// ResultTexturesFiller.hlsl
		{
			struct InputTextures
			{
				UINT DisplacementsTexturesIdx;
				UINT DerivativesTexturesIdx;
				UINT TurbulenceTexturesIdx;

				UINT DxDzTexturesIdx;
				UINT DyDxzTexturesIdx;
				UINT DyxDyzTexturesIdx;
				UINT DxxDzzTexturesIdx;
			} inputTextures;

			inputTextures.DisplacementsTexturesIdx = m_DisplacementsTextures->GetUAVView()->GetGpuHeapIndex();
			inputTextures.DerivativesTexturesIdx = m_DerivativesTextures->GetUAVView()->GetGpuHeapIndex();
			inputTextures.TurbulenceTexturesIdx = m_TurbulenceTextures->GetUAVView()->GetGpuHeapIndex();
			inputTextures.DxDzTexturesIdx = m_DxDzTextures->GetUAVView()->GetGpuHeapIndex();
			inputTextures.DyDxzTexturesIdx = m_DyDxzTextures->GetUAVView()->GetGpuHeapIndex();
			inputTextures.DyxDyzTexturesIdx = m_DyxDyzTextures->GetUAVView()->GetGpuHeapIndex();
			inputTextures.DxxDzzTexturesIdx = m_DxxDzzTextures->GetUAVView()->GetGpuHeapIndex();

			buffDesc = {};
			buffDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			buffDesc.Width = sizeof(InputTextures);
			buffDesc.Alignment = 0;
			buffDesc.Height = 1;
			buffDesc.DepthOrArraySize = 1;
			buffDesc.MipLevels = 1;
			buffDesc.SampleDesc.Count = 1;
			buffDesc.SampleDesc.Quality = 0;
			buffDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			buffDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

			m_ResultTexturesBuffer = std::make_shared<BufferD3D12>(device, context, buffDesc, QueueId::Direct);
			m_ResultTexturesBuffer->SetName(L"ResultTexturesBuffer");
			m_ResultTexturesBuffer->LoadData(context, &inputTextures);
		}

		m_MeshGenerator = std::make_unique<MeshGenerator>(device, context, 512, 512, 5000);

		ShaderResourceDesc resDesc[] =
		{
			{ "cbPass", SHADER_RESOURCE_TYPE_DYNAMIC },
			{ "cbPassData", SHADER_RESOURCE_TYPE_DYNAMIC },
		};

		ShaderDesc sDesc = {};
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		sDesc.ResourceNum = _countof(resDesc);
		sDesc.ResourceDesc = resDesc;

		auto vsDraw = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Water\\FFTOceanDraw.hlsl", L"VS", L"vs_6_6", nullptr, sDesc);
		auto hsDraw = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Water\\FFTOceanDraw.hlsl", L"Hull", L"hs_6_6", nullptr, sDesc);
		auto dsDraw = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Water\\FFTOceanDraw.hlsl", L"Domain", L"ds_6_6", nullptr, sDesc);
		auto psDraw = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Water\\FFTOceanDraw.hlsl", L"PS", L"ps_6_6", nullptr, sDesc);

		auto calculateInitialSpectrumTexturesCS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Water\\InitialSpectrum.hlsl", L"CalculateInitialSpectrumTextures", L"cs_6_6", nullptr, sDesc);
		auto calculateConjugatedInitialSpectrumTexturesCS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Water\\InitialSpectrum.hlsl", L"CalculateConjugatedInitialSpectrumTextures", L"cs_6_6", nullptr, sDesc);

		auto fillResultTexturesCS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Water\\ResultTexturesFiller.hlsl", L"FillResultTextures", L"cs_6_6", nullptr, sDesc);

		auto calculateTimeDependentComplexAmplitudesAndDerivativesCS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Water\\TimeDependentSpectrum.hlsl", L"CalculateTimeDependentComplexAmplitudesAndDerivatives", L"cs_6_6", nullptr, sDesc);

		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(device);

		D3D12_DEPTH_STENCIL_DESC dss = {};
		dss.DepthEnable = TRUE;
		dss.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
		dss.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

		D3D12_INPUT_ELEMENT_DESC elementDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
		D3D12_INPUT_LAYOUT_DESC inputLayout = {};
		inputLayout.NumElements = _countof(elementDesc);
		inputLayout.pInputElementDescs = elementDesc;

		m_DrawPSO.SetShader(vsDraw);
		m_DrawPSO.SetShader(psDraw);
		m_DrawPSO.SetShader(hsDraw);
		m_DrawPSO.SetShader(dsDraw);
		m_DrawPSO.SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH);
		m_DrawPSO.SetRTVFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
		m_DrawPSO.SetDepthStencilState(dss);
		m_DrawPSO.SetInputLayout(inputLayout);
		m_DrawPSO.Build(device);
		m_DrawBinder = m_DrawPSO.CreateShaderBinder();

		m_InitialSpectrumTexturesPSO = std::make_unique<ComputePipelineState>(QueueId::Direct);
		m_InitialSpectrumTexturesPSO->SetShader(calculateInitialSpectrumTexturesCS);
		m_InitialSpectrumTexturesPSO->Build(device);
		m_InitialSpectrumTexturesPSO->SetName(L"InitialSpectrumTexturesPSO");
		m_InitialSpectrumTexturesBinder = m_InitialSpectrumTexturesPSO->CreateShaderBinder();

		m_ConjugatedInitialSpectrumTexturesPSO = std::make_unique<ComputePipelineState>(QueueId::Direct);
		m_ConjugatedInitialSpectrumTexturesPSO->SetShader(calculateConjugatedInitialSpectrumTexturesCS);
		m_ConjugatedInitialSpectrumTexturesPSO->Build(device);
		m_ConjugatedInitialSpectrumTexturesPSO->SetName(L"ConjugatedInitialSpectrumTexturesPSO");
		m_ConjugatedInitialSpectrumTexturesBinder = m_ConjugatedInitialSpectrumTexturesPSO->CreateShaderBinder();

		m_FillResultTexturesPSO = std::make_unique<ComputePipelineState>(QueueId::Direct);
		m_FillResultTexturesPSO->SetShader(fillResultTexturesCS);
		m_FillResultTexturesPSO->Build(device);
		m_FillResultTexturesPSO->SetName(L"FillResultTexturesPSO");
		m_FillResultTexturesBinder = m_FillResultTexturesPSO->CreateShaderBinder();

		m_TimeDependentComplexAmplitudesAndDerivativesPSO = std::make_unique<ComputePipelineState>(QueueId::Direct);
		m_TimeDependentComplexAmplitudesAndDerivativesPSO->SetShader(calculateTimeDependentComplexAmplitudesAndDerivativesCS);
		m_TimeDependentComplexAmplitudesAndDerivativesPSO->Build(device);
		m_TimeDependentComplexAmplitudesAndDerivativesPSO->SetName(L"TimeDependentComplexAmplitudesAndDerivativesPSO");
		m_TimeDependentComplexAmplitudesAndDerivativesBinder = m_TimeDependentComplexAmplitudesAndDerivativesPSO->CreateShaderBinder();

		m_DrawBinder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_PassBuffer);
		m_DrawBinder->BindDynamicResource(EDU_SHADER_TYPE_HULL, "cbPass", m_PassBuffer);
		m_DrawBinder->BindDynamicResource(EDU_SHADER_TYPE_DOMAIN, "cbPass", m_PassBuffer);
		m_DrawBinder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_PassBuffer);
		m_DrawBinder->BindResource(EDU_SHADER_TYPE_HULL, "cbConstants", m_DrawConstantsBuffer);
		m_DrawBinder->BindResource(EDU_SHADER_TYPE_DOMAIN, "cbConstants", m_DrawConstantsBuffer);
		m_DrawBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "cbConstants", m_DrawConstantsBuffer);

		m_InitialSpectrumTexturesBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbConstants", m_ComputeConstantsBuffer);
		m_InitialSpectrumTexturesBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbInitSpectrumData", m_InitSpectrumBuffer);

		m_ConjugatedInitialSpectrumTexturesBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbConstants", m_ComputeConstantsBuffer);
		m_ConjugatedInitialSpectrumTexturesBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbInitSpectrumData", m_InitSpectrumBuffer);

		m_FillResultTexturesBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbConstants", m_ComputeConstantsBuffer);
		m_FillResultTexturesBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbInputTextures", m_ResultTexturesBuffer);

		m_TimeDependentComplexAmplitudesAndDerivativesBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbConstants", m_ComputeConstantsBuffer);
		m_TimeDependentComplexAmplitudesAndDerivativesBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbInputTextures", m_TimeDependentBuffer);
		m_TimeDependentComplexAmplitudesAndDerivativesBinder->BindDynamicResource(EDU_SHADER_TYPE_COMPUTE, "cbPassData", m_PassBuffer);

		CalculateInitialSpectrum();
		UpdateDrawSettings(m_DrawSettings);
	}

	void FFTOcean::Compute(float time)
	{
		m_PassBuffer->LoadData(m_Context, time);

		m_TimeDependentComplexAmplitudesAndDerivativesPSO->BeginPSOAndCommitResources(m_Context, m_TimeDependentComplexAmplitudesAndDerivativesBinder.get());
		m_Context->GetCommandCtx()->GetCmdList()->Dispatch(m_TextureSize / LOCAL_WORK_GROUPS_X, m_TextureSize / LOCAL_WORK_GROUPS_Y, 1);

		m_Context->GetCommandCtx()->TransitionResource(m_DxDzTextures.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_Context->GetCommandCtx()->TransitionResource(m_DyDxzTextures.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_Context->GetCommandCtx()->TransitionResource(m_DyxDyzTextures.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_Context->GetCommandCtx()->TransitionResource(m_DxxDzzTextures.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

		m_IFFT->InverseFastFourierTransform(m_DxDzTextures.get());
		m_IFFT->InverseFastFourierTransform(m_DyDxzTextures.get());
		m_IFFT->InverseFastFourierTransform(m_DyxDyzTextures.get());
		m_IFFT->InverseFastFourierTransform(m_DxxDzzTextures.get());

		m_Context->GetCommandCtx()->TransitionResource(m_DxDzTextures.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_Context->GetCommandCtx()->TransitionResource(m_DyDxzTextures.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_Context->GetCommandCtx()->TransitionResource(m_DyxDyzTextures.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_Context->GetCommandCtx()->TransitionResource(m_DxxDzzTextures.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

		m_FillResultTexturesPSO->BeginPSOAndCommitResources(m_Context, m_FillResultTexturesBinder.get());
		m_Context->GetCommandCtx()->GetCmdList()->Dispatch(m_TextureSize / LOCAL_WORK_GROUPS_X, m_TextureSize / LOCAL_WORK_GROUPS_Y, 1);

		m_Context->GetCommandCtx()->TransitionResource(m_DisplacementsTextures.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
	}

	void FFTOcean::Render(Camera* camera, XMFLOAT3 sunPos, XMFLOAT3 sunColor)
	{
		struct PassData
		{
			XMFLOAT4X4 World;
			XMFLOAT4X4 ViewProj;
			XMFLOAT3 CameraPos;
			UINT Padding0;
			XMFLOAT3 MainLightPos;
			UINT Padding1;
			XMFLOAT3 MainLightColor;
		} passData;

		XMStoreFloat4x4(&passData.World, XMMatrixIdentity());
		XMStoreFloat4x4(&passData.ViewProj, XMMatrixTranspose(camera->GetViewProjMatrix()));
		passData.CameraPos = camera->GetPosition();
		passData.MainLightPos = sunPos;
		passData.MainLightColor = sunColor;

		m_PassBuffer->LoadData(m_Context, passData);

		m_DrawPSO.BeginPSOAndCommitResources(m_Context, m_DrawBinder.get());

		m_Context->GetCommandCtx()->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
		m_Context->GetCommandCtx()->GetCmdList()->IASetVertexBuffers(0, 1, &m_MeshGenerator->GetVertexBufferView());
		m_Context->GetCommandCtx()->GetCmdList()->IASetIndexBuffer(&m_MeshGenerator->GetIndexBufferView());

		m_Context->GetCommandCtx()->GetCmdList()->DrawIndexedInstanced(m_MeshGenerator->GetIndexCount(), 1, 0, 0, 0);
	}

	void FFTOcean::UpdateComputeSettings(FFTOceanComputeSettings newSettings)
	{
		if (memcmp(&newSettings, &m_ComputeSettings, sizeof(FFTOceanComputeSettings)) != 0)
		{
			m_ComputeSettings = newSettings;
			CalculateInitialSpectrum();
		}
	}

	void FFTOcean::UpdateDrawSettings(FFTOceanDrawSettings newSettings)
	{
		m_DrawSettings = newSettings;
		DrawConstantData drawData = {};

		drawData.NbCascades = m_CascadesCount;
		drawData.WavelengthsIdx = m_Wavelengths->GetSRVView()->GetGpuHeapIndex();
		drawData.DisplacementsTexturesIdx = m_DisplacementsTextures->GetSRVView()->GetGpuHeapIndex();
		drawData.DerivativesTexturesIdx = m_DerivativesTextures->GetSRVView()->GetGpuHeapIndex();
		drawData.TurbulenceTexturesIdx = m_TurbulenceTextures->GetSRVView()->GetGpuHeapIndex();
		drawData.ReflectionCubeIdx = m_AtmosphereCube->GetSRVView()->GetGpuHeapIndex();

		drawData.MaxLODLevel = newSettings.MaxLODLevel;
		drawData.TesselationLevel = newSettings.TesselationLevel;
		drawData.MaxTesselationDistance = newSettings.MaxTesselationDistance;
		drawData.TesselationDecayFactor = newSettings.TesselationDecayFactor;
		drawData.CullingTollerance = newSettings.CullingTollerance;

		drawData.EnvironmentReflectionStrength = newSettings.EnvironmentReflectionStrength;
		drawData.SubsurfaceScatteringColor = newSettings.SubsurfaceScatteringColor;
		drawData.SubsurfaceScatteringIntensity = newSettings.SubsurfaceScatteringIntensity;
		drawData.DeepWaterColor = newSettings.DeepWaterColor;
		drawData.WaterFogDensity = newSettings.WaterFogDensity;
		drawData.RefractionStrength = newSettings.RefractionStrength;
		drawData.Roughness = newSettings.Roughness;
		drawData.AnisoEX = newSettings.AnisoEX;
		drawData.AnisoEY = newSettings.AnisoEY;
		drawData.FoamBlending = newSettings.FoamBlending;
		drawData.FoamThreshold = newSettings.FoamThreshold;
		drawData.FoamColor = newSettings.FoamColor;
		drawData.ShadowsColor = newSettings.ShadowsColor;
		drawData.ShadowsIntensity = newSettings.ShadowsIntensity;
		drawData.SunReflectionStrength = newSettings.SunReflectionStrength;

		m_DrawConstantsBuffer->LoadData(m_Context, &drawData);
	}

	void FFTOcean::CalculateInitialSpectrum()
	{
		InitialSpectrumData initialSpectrumData = {};
		initialSpectrumData.WindSpeed = m_ComputeSettings.WindSpeed;
		initialSpectrumData.WindDirectionX = m_ComputeSettings.WindDirectionX;
		initialSpectrumData.WindDirectionY = m_ComputeSettings.WindDirectionY;
		initialSpectrumData.Gravity = m_ComputeSettings.Gravity;
		initialSpectrumData.Fetch = m_ComputeSettings.Fetch;
		initialSpectrumData.Depth = m_ComputeSettings.Depth;

		initialSpectrumData.RandomNoiseTextureIdx = m_RandomNoiseTexture->GetSRVView()->GetGpuHeapIndex();
		initialSpectrumData.InitialSpectrumTexturesIdx = m_InitialSpectrumTextures->GetUAVView()->GetGpuHeapIndex();
		initialSpectrumData.WavesDataTexturesIdx = m_WavesDataTextures->GetUAVView()->GetGpuHeapIndex();
		initialSpectrumData.WavelengthsIdx = m_Wavelengths->GetSRVView()->GetGpuHeapIndex();
		initialSpectrumData.CutoffsIdx = m_Cutoffs->GetSRVView()->GetGpuHeapIndex();
		initialSpectrumData.FadesIdx = m_Fades->GetSRVView()->GetGpuHeapIndex();
		initialSpectrumData.SwellsIdx = m_Swells->GetSRVView()->GetGpuHeapIndex();

		m_InitSpectrumBuffer->LoadData(m_Context, &initialSpectrumData);

		// Calculate the initial spectrum H0(K)
		m_InitialSpectrumTexturesPSO->BeginPSOAndCommitResources(m_Context, m_InitialSpectrumTexturesBinder.get());
		m_Context->GetCommandCtx()->GetCmdList()->Dispatch(m_TextureSize / LOCAL_WORK_GROUPS_X, m_TextureSize / LOCAL_WORK_GROUPS_Y, 1);

		// Store, in each element on the texture, the value of the complex conjugate element
		// Now the Initial spectrum texture stores H0(K) and H0(-k)*
		m_ConjugatedInitialSpectrumTexturesPSO->BeginPSOAndCommitResources(m_Context, m_ConjugatedInitialSpectrumTexturesBinder.get());
		m_Context->GetCommandCtx()->GetCmdList()->Dispatch(m_TextureSize / LOCAL_WORK_GROUPS_X, m_TextureSize / LOCAL_WORK_GROUPS_Y, 1);
	}

	// Generates a random number from a Normal Distribution N(0, 1)
	// Why Normal Distribution? Because the initial spectrum H0(K) is defined as a function of a Gaussian random variable,
	// as explained in https://doi.org/10.15480/882.1436 (formula 3.6)
	// or in https://www.researchgate.net/publication/264839743_Simulating_Ocean_Water (formula 43)
	float GenerateRandomNumber()
	{
		static thread_local std::mt19937 eng(std::random_device{}());
		static thread_local std::normal_distribution<float> distr(0.0f, 1.0f);

		return distr(eng);
	}

	// Generates a 2D Texture where each pixel contains a Vector4, x and y are random numbers from -1 to 1 and z and w are 0
	// This texture is generated on the CPU because we don't need to generate new random noise when the ocean parameters change
	// So this texture is generated only once and at the start of the game execution
	void FFTOcean::GenerateRandomNoiseTexture(RenderDeviceD3D12* device, DeviceContext* context)
	{
		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.MipLevels = 1;
		texDesc.DepthOrArraySize = 1;
		texDesc.Width = m_TextureSize;
		texDesc.Height = m_TextureSize;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
		texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		m_RandomNoiseTexture = std::make_shared<TextureD3D12>(device, texDesc, nullptr, QueueId::Direct);
		m_RandomNoiseTexture->CreateSRV(&srvDesc, false);
		m_RandomNoiseTexture->SetName(L"RandomNoiseTexture");

		struct Pixel
		{
			float r;
			float g;
		};

		std::vector<Pixel> data(m_TextureSize * m_TextureSize);
		for (int i = 0; i < m_TextureSize; i++)
		{
			for (int j = 0; j < m_TextureSize; j++)
			{
				data[i * m_TextureSize + j] = { GenerateRandomNumber(), GenerateRandomNumber() };
			}
		}

		m_RandomNoiseTexture->LoadData(context, data.data());
	}
}