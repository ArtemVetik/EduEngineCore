#include "FFTOcean.h"

#include <cmath>
#include <random>

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

	FFTOcean::FFTOcean(RenderDeviceD3D12* device, DeviceContext* context, InitialSettings initialSettings) :
		m_Context(context),
		m_NbCascades(initialSettings.CascadesCount),
		m_TextureSize(initialSettings.TextureSize)
	{
		struct ConstantData
		{
			UINT NbCascades;
			UINT TextureSize;
		};

		D3D12_RESOURCE_DESC buffDesc = {};
		buffDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		buffDesc.Width = sizeof(ConstantData);
		buffDesc.Alignment = 0;
		buffDesc.Height = 1;
		buffDesc.DepthOrArraySize = 1;
		buffDesc.MipLevels = 1;
		buffDesc.SampleDesc.Count = 1;
		buffDesc.SampleDesc.Quality = 0;
		buffDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		buffDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		ConstantData constantsData = {};
		constantsData.NbCascades = m_NbCascades;
		constantsData.TextureSize = m_TextureSize;

		m_ConstantsBuffer = std::make_shared<BufferD3D12>(device, context, buffDesc, QueueId::Direct);
		m_ConstantsBuffer->LoadData(context, &constantsData);

		m_IFFT = std::make_unique<IFFT>(device, context, m_TextureSize, 1, m_ConstantsBuffer);

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
				texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

				D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
				uavDesc.Format = texDesc.Format;
				uavDesc.Texture2DArray.ArraySize = depth;
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

		CreateTexture(m_NbCascades, DXGI_FORMAT_R32G32B32A32_FLOAT, m_WavesDataTextures, L"WavesDataTextures", true);
		CreateTexture(m_NbCascades, DXGI_FORMAT_R32G32B32A32_FLOAT, m_InitialSpectrumTextures, L"InitialSpectrumTextures", true);
		CreateTexture(m_NbCascades, DXGI_FORMAT_R32G32B32A32_FLOAT, m_DisplacementsTextures, L"DisplacementsTextures", true);
		CreateTexture(m_NbCascades, DXGI_FORMAT_R32G32B32A32_FLOAT, m_DerivativesTextures, L"DerivativesTextures");
		CreateTexture(m_NbCascades, DXGI_FORMAT_R32G32B32A32_FLOAT, m_TurbulenceTextures, L"TurbulenceTextures");
		CreateTexture(m_NbCascades, DXGI_FORMAT_R32G32_FLOAT, m_DxDzTextures, L"DxDzTextures");
		CreateTexture(m_NbCascades, DXGI_FORMAT_R32G32_FLOAT, m_DyDxzTextures, L"DyDxzTextures");
		CreateTexture(m_NbCascades, DXGI_FORMAT_R32G32_FLOAT, m_DyxDyzTextures, L"DyxDyzTextures");
		CreateTexture(m_NbCascades, DXGI_FORMAT_R32G32_FLOAT, m_DxxDzzTextures, L"DxxDzzTextures");

		float wavelengths[MaxCascades];
		float cutoffs[MaxCascades * 2];
		float swells[MaxCascades];
		float fades[MaxCascades];

		for (int i = 0; i < m_NbCascades; i++) {
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

		CreateBuffer(m_NbCascades, m_Wavelengths, wavelengths, L"Wavelengths");
		CreateBuffer(m_NbCascades * 2, m_Cutoffs, cutoffs, L"Cutoffs");
		CreateBuffer(m_NbCascades, m_Swells, swells, L"Swells");
		CreateBuffer(m_NbCascades, m_Fades, fades, L"Fades");

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

		ShaderResourceDesc resDesc[] =
		{
			{ "cbPassData", SHADER_RESOURCE_TYPE_DYNAMIC },
		};

		ShaderDesc sDesc = {};
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		sDesc.ResourceNum = _countof(resDesc);
		sDesc.ResourceDesc = resDesc;

		auto calculateInitialSpectrumTexturesCS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Water\\InitialSpectrum.hlsl", L"CalculateInitialSpectrumTextures", L"cs_6_6", nullptr, sDesc);
		auto calculateConjugatedInitialSpectrumTexturesCS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Water\\InitialSpectrum.hlsl", L"CalculateConjugatedInitialSpectrumTextures", L"cs_6_6", nullptr, sDesc);

		auto fillResultTexturesCS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Water\\ResultTexturesFiller.hlsl", L"FillResultTextures", L"cs_6_6", nullptr, sDesc);

		auto calculateTimeDependentComplexAmplitudesAndDerivativesCS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Water\\TimeDependentSpectrum.hlsl", L"CalculateTimeDependentComplexAmplitudesAndDerivatives", L"cs_6_6", nullptr, sDesc);

		m_TimeBuffer = std::make_shared<DynamicUploadBuffer>(device);

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

		m_InitialSpectrumTexturesBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbConstants", m_ConstantsBuffer);
		m_InitialSpectrumTexturesBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbInitSpectrumData", m_InitSpectrumBuffer);

		m_ConjugatedInitialSpectrumTexturesBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbConstants", m_ConstantsBuffer);
		m_ConjugatedInitialSpectrumTexturesBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbInitSpectrumData", m_InitSpectrumBuffer);

		m_FillResultTexturesBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbConstants", m_ConstantsBuffer);
		m_FillResultTexturesBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbInputTextures", m_ResultTexturesBuffer);

		m_TimeDependentComplexAmplitudesAndDerivativesBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbConstants", m_ConstantsBuffer);
		m_TimeDependentComplexAmplitudesAndDerivativesBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbInputTextures", m_TimeDependentBuffer);
		m_TimeDependentComplexAmplitudesAndDerivativesBinder->BindDynamicResource(EDU_SHADER_TYPE_COMPUTE, "cbPassData", m_TimeBuffer);

		CalculateInitialSpectrum(m_Settings);
	}

	void FFTOcean::Update(float time)
	{
		m_TimeBuffer->LoadData(m_Context, time);

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

	void FFTOcean::UpdateSettings(Settings newSettings)
	{
		if (memcmp(&newSettings, &m_Settings, sizeof(Settings)) != 0)
		{
			m_Settings = newSettings;
			CalculateInitialSpectrum(m_Settings);
		}
	}

	void FFTOcean::CalculateInitialSpectrum(Settings settings)
	{
		InitialSpectrumData initialSpectrumData = {};
		initialSpectrumData.WindSpeed = settings.WindSpeed;
		initialSpectrumData.WindDirectionX = settings.WindDirectionX;
		initialSpectrumData.WindDirectionY = settings.WindDirectionY;
		initialSpectrumData.Gravity = settings.Gravity;
		initialSpectrumData.Fetch = settings.Fetch;
		initialSpectrumData.Depth = settings.Depth;

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

		std::vector<Pixel> data(m_TextureSize* m_TextureSize);
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