#include "IFFT.h"

namespace EduEngine
{
	const int LOCAL_WORK_GROUPS_X = 8;
	const int LOCAL_WORK_GROUPS_Y = 8;

	IFFT::IFFT(RenderDeviceD3D12* device, DeviceContext* context, uint32 texturesSize, uint32 nbCascades, std::shared_ptr<BufferD3D12> constantBuffer) :
		m_HorizontalStepIFFTPSO(QueueId::Direct),
		m_VerticalStepIFFTPSO(QueueId::Direct),
		m_PermutePSO(QueueId::Direct),
		m_TexturesSize(texturesSize),
		m_Context(context)
	{
		ShaderResourceDesc resDesc[] =
		{
			{ "cbFFTPerPass", SHADER_RESOURCE_TYPE_DYNAMIC },
		};

		ShaderDesc sDesc = {};
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		sDesc.ResourceNum = _countof(resDesc);
		sDesc.ResourceDesc = resDesc;

		auto precomputeTwiddleFactorsAndInputIndicesCS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Water\\IFFT.hlsl", L"PrecomputeTwiddleFactorsAndInputIndices", L"cs_6_6", nullptr, sDesc);
		auto horizontalStepIFFTCS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Water\\IFFT.hlsl", L"HorizontalStepIFFT", L"cs_6_6", nullptr, sDesc);
		auto verticalStepIFFTCSW = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Water\\IFFT.hlsl", L"VerticalStepIFFT", L"cs_6_6", nullptr, sDesc);
		auto permuteCS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Water\\IFFT.hlsl", L"Permute", L"cs_6_6", nullptr, sDesc);

		struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) FFTData
		{
			UINT TwiddleFactorsAndInputIndicesTextureIdx;
			UINT PingPongTexturesIdx;
		};

		D3D12_RESOURCE_DESC buffDesc = {};
		buffDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		buffDesc.Width = sizeof(FFTData);
		buffDesc.Alignment = 0;
		buffDesc.Height = 1;
		buffDesc.DepthOrArraySize = 1;
		buffDesc.MipLevels = 1;
		buffDesc.SampleDesc.Count = 1;
		buffDesc.SampleDesc.Quality = 0;
		buffDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		buffDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		m_IFFTDataBuffer = std::make_shared<BufferD3D12>(device, context, buffDesc, QueueId::Direct);

		// Create the texture that will store the twiddle factors and input indices for the Cooley-Tukey algorithm.
		// https://doi.org/10.15480/882.1436 ("4.2.6 Butterfly Texture" section)
		int logSize = (int)log2(texturesSize);

		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.MipLevels = 1;
		texDesc.DepthOrArraySize = 1;
		texDesc.Width = logSize;
		texDesc.Height = texturesSize;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Format = texDesc.Format;
		uavDesc.Texture2D.MipSlice = 0;
		uavDesc.Texture2D.PlaneSlice = 0;

		m_TwiddleFactorsAndInputIndicesTexture = std::make_shared<TextureD3D12>(device, texDesc, nullptr, QueueId::Direct);
		m_TwiddleFactorsAndInputIndicesTexture->CreateUAV(&uavDesc, false);
		m_TwiddleFactorsAndInputIndicesTexture->SetName(L"TwiddleFactorsAndInputIndicesTexture");

		// Create the "Ping Pong" textures that will store the intermediate IFFT computations.
		// One "Ping Pong" texture for each cascade
		// https://doi.org/10.15480/882.1436 ("4.2.5 Ping-Pong Texture" section)
		{
			D3D12_RESOURCE_DESC texDesc = {};
			texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			texDesc.Alignment = 0;
			texDesc.MipLevels = 1;
			texDesc.DepthOrArraySize = nbCascades;
			texDesc.Width = texturesSize;
			texDesc.Height = texturesSize;
			texDesc.SampleDesc.Count = 1;
			texDesc.SampleDesc.Quality = 0;
			texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			uavDesc.Format = texDesc.Format;
			uavDesc.Texture2DArray.ArraySize = nbCascades;
			uavDesc.Texture2DArray.FirstArraySlice = 0;
			uavDesc.Texture2DArray.MipSlice = 0;
			uavDesc.Texture2DArray.PlaneSlice = 0;

			m_PingPongTextures = std::make_shared<TextureD3D12>(device, texDesc, nullptr, QueueId::Direct);
			m_PingPongTextures->CreateUAV(&uavDesc, false);
			m_PingPongTextures->SetName(L"PingPongTextures");
		}

		FFTData data = {};
		data.TwiddleFactorsAndInputIndicesTextureIdx = m_TwiddleFactorsAndInputIndicesTexture->GetUAVView()->GetGpuHeapIndex();
		data.PingPongTexturesIdx = m_PingPongTextures->GetUAVView()->GetGpuHeapIndex();
		m_IFFTDataBuffer->LoadData(context, &data);

		//
		// Precompute the twiddle factors and input indices and store them in a texture,
		// as explained in https://doi.org/10.15480/882.1436 ("4.2.6 Butterfly Texture" section).
		//
		ComputePipelineState precomputeTwiddleFactorsAndInputIndicesPSO(QueueId::Direct);
		precomputeTwiddleFactorsAndInputIndicesPSO.SetShader(precomputeTwiddleFactorsAndInputIndicesCS);
		precomputeTwiddleFactorsAndInputIndicesPSO.Build(device);
		precomputeTwiddleFactorsAndInputIndicesPSO.SetName(L"PrecomputeTwiddleFactorsAndInputIndicesPSO");

		auto binder = precomputeTwiddleFactorsAndInputIndicesPSO.CreateShaderBinder();
		binder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbConstants", constantBuffer);
		binder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbFFTData", m_IFFTDataBuffer);

		precomputeTwiddleFactorsAndInputIndicesPSO.BeginPSOAndCommitResources(context, binder.get());
		context->GetCommandCtx()->GetCmdList()->Dispatch(logSize, texturesSize / 2 / LOCAL_WORK_GROUPS_Y, 1);

		//
		// Build PSO and binders
		//
		m_HorizontalStepIFFTPSO.SetShader(horizontalStepIFFTCS);
		m_HorizontalStepIFFTPSO.Build(device);
		m_HorizontalStepIFFTPSO.SetName(L"HorizontalStepIFFTPSO");
		m_HorizontalStepIFFTBinder = m_HorizontalStepIFFTPSO.CreateShaderBinder();

		m_VerticalStepIFFTPSO.SetShader(verticalStepIFFTCSW);
		m_VerticalStepIFFTPSO.Build(device);
		m_VerticalStepIFFTPSO.SetName(L"VerticalStepIFFTPSO");
		m_VerticalStepIFFTBinder = m_VerticalStepIFFTPSO.CreateShaderBinder();

		m_PermutePSO.SetShader(permuteCS);
		m_PermutePSO.Build(device);
		m_PermutePSO.SetName(L"PermutePSO");
		m_PermuteBinder = m_PermutePSO.CreateShaderBinder();

		m_IFFTPassBuffer = std::make_shared<DynamicUploadBuffer>(device);

		m_HorizontalStepIFFTBinder->BindDynamicResource(EDU_SHADER_TYPE_COMPUTE, "cbFFTPerPass", m_IFFTPassBuffer);
		m_HorizontalStepIFFTBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbConstants", constantBuffer);
		m_HorizontalStepIFFTBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbFFTData", m_IFFTDataBuffer);

		m_VerticalStepIFFTBinder->BindDynamicResource(EDU_SHADER_TYPE_COMPUTE, "cbFFTPerPass", m_IFFTPassBuffer);
		m_VerticalStepIFFTBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbConstants", constantBuffer);
		m_VerticalStepIFFTBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbFFTData", m_IFFTDataBuffer);

		m_PermuteBinder->BindDynamicResource(EDU_SHADER_TYPE_COMPUTE, "cbFFTPerPass", m_IFFTPassBuffer);
		m_PermuteBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbConstants", constantBuffer);
	}

	// Executes the IFFT on the input texture array, as explained in https://doi.org/10.15480/882.1436 (Chapter 2 and "4.2.1 IFFT Algorithm" section).
	void IFFT::InverseFastFourierTransform(TextureD3D12* inputTextureArray)
	{
		int logSize = (int)log2(m_TexturesSize);
		bool pingPong = false;

		struct FFTPassData
		{
			UINT InputTexturesIdx;
			UINT Step;
			UINT PingPong;
		} passData;

		passData.InputTexturesIdx = inputTextureArray->GetUAVView()->GetGpuHeapIndex();

		for (int i = 0; i < logSize; i++)
		{
			passData.Step = i;
			passData.PingPong = pingPong;
			m_IFFTPassBuffer->LoadData(m_Context, passData);

			m_HorizontalStepIFFTPSO.BeginPSOAndCommitResources(m_Context, m_HorizontalStepIFFTBinder.get());
			m_Context->GetCommandCtx()->GetCmdList()->Dispatch(m_TexturesSize / LOCAL_WORK_GROUPS_X, m_TexturesSize / LOCAL_WORK_GROUPS_Y, 1);

			if (!pingPong)
				m_Context->GetCommandCtx()->TransitionResource(inputTextureArray, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
			else
				m_Context->GetCommandCtx()->TransitionResource(m_PingPongTextures.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

			pingPong = !pingPong;
		}

		for (int i = 0; i < logSize; i++)
		{
			passData.Step = i;
			passData.PingPong = pingPong;
			m_IFFTPassBuffer->LoadData(m_Context, passData);

			m_VerticalStepIFFTPSO.BeginPSOAndCommitResources(m_Context, m_VerticalStepIFFTBinder.get());
			m_Context->GetCommandCtx()->GetCmdList()->Dispatch(m_TexturesSize / LOCAL_WORK_GROUPS_X, m_TexturesSize / LOCAL_WORK_GROUPS_Y, 1);

			if (!pingPong)
				m_Context->GetCommandCtx()->TransitionResource(inputTextureArray, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
			else
				m_Context->GetCommandCtx()->TransitionResource(m_PingPongTextures.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

			pingPong = !pingPong;
		}

		m_PermutePSO.BeginPSOAndCommitResources(m_Context, m_PermuteBinder.get());
		m_Context->GetCommandCtx()->GetCmdList()->Dispatch(m_TexturesSize / LOCAL_WORK_GROUPS_X, m_TexturesSize / LOCAL_WORK_GROUPS_Y, 1);
	}
}