#include "Atmosphere.h"

namespace EduEngine
{
	// TODO: Change naming style
	const int LOCAL_WORK_GROUPS_X = 8;
	const int LOCAL_WORK_GROUPS_Y = 8;

	const int transmittanceLUTWidth = 64;
	const int transmittanceLUTHeight = 256;

	const int multiscatteringLUTWidth = 64;
	const int multiscatteringLUTHeight = 64;

	const int skyViewLUTWidth = 256;
	const int skyViewLUTHeight = 128;

	const int reflectionMapSize = 256;

	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) LUTSizes
	{
		UINT LutWidth;
		UINT LutHeight;
	};

	struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) AtmosphereParams
	{
		float PlanetRadius;
		float AtmosphereRadius;

		float MieG;
		UINT Padding0;

		XMFLOAT3 RayleighScatteringCoefficient;
		float RayleighScaleHeight;
		XMFLOAT3 RayleighAbsorptionCoefficient;
		UINT Padding1;

		XMFLOAT3 MieScatteringCoefficient;
		float MieScaleHeight;
		XMFLOAT3 MieAbsorptionCoefficient;
		UINT Padding2;

		XMFLOAT3 OzoneScatteringCoefficient;
		UINT Padding3;
		XMFLOAT3 OzoneAbsorptionCoefficient;
		UINT Padding4;

		XMFLOAT3 GroundSpectrumAlbedo;
		UINT Padding5;
	};

	Atmosphere::Atmosphere(RenderDeviceD3D12* device, DeviceContext* context) :
		m_Device(device),
		m_Context(context),
		m_SunColor(1, 1, 1, 1)
	{
		ShaderResourceDesc resDesc[] =
		{
			{ "cbPass", SHADER_RESOURCE_TYPE_DYNAMIC },
		};

		ShaderDesc sDesc = {};
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		sDesc.ResourceDesc = resDesc;
		sDesc.ResourceNum = _countof(resDesc);

		auto computeMultiscatteringLUTCS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Atmosphere\\MultiscatteringLUT.hlsl", L"ComputeMultiscatteringLUT", L"cs_6_6", nullptr, sDesc);
		auto computeSkyViewLUTCS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Atmosphere\\SkyViewLUT.hlsl", L"ComputeSkyViewLUT", L"cs_6_6", nullptr, sDesc);
		auto computeTransmittanceLUTCS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Atmosphere\\TransmittanceLUT.hlsl", L"ComputeTransmittanceLUT", L"cs_6_6", nullptr, sDesc);

		m_MultiscatteringLUTPso = std::make_unique<ComputePipelineState>(QueueId::Direct);
		m_MultiscatteringLUTPso->SetShader(computeMultiscatteringLUTCS);
		m_MultiscatteringLUTPso->Build(device);
		m_MultiscatteringLUTPso->SetName(L"MultiscatteringLUT_PSO");
		m_MultiscatteringLUTBinder = m_MultiscatteringLUTPso->CreateShaderBinder();

		m_SkyViewLUTPso = std::make_unique<ComputePipelineState>(QueueId::Direct);
		m_SkyViewLUTPso->SetShader(computeSkyViewLUTCS);
		m_SkyViewLUTPso->Build(device);
		m_SkyViewLUTPso->SetName(L"SkyViewLUT_PSO");
		m_SkyViewLUTBinder = m_SkyViewLUTPso->CreateShaderBinder();

		m_TransmittanceLUTPso = std::make_unique<ComputePipelineState>(QueueId::Direct);
		m_TransmittanceLUTPso->SetShader(computeTransmittanceLUTCS);
		m_TransmittanceLUTPso->Build(device);
		m_TransmittanceLUTPso->SetName(L"TransmittanceLUT_PSO");
		m_TransmittanceLUTBinder = m_TransmittanceLUTPso->CreateShaderBinder();

		auto CreateTexture = [&](std::shared_ptr<TextureD3D12>& texture,
								 uint32 width,
								 uint32 height,
								 DXGI_FORMAT format,
								 const wchar_t* name,
								 bool createSrv = false
			)
			{
				D3D12_RESOURCE_DESC texDesc = {};
				texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				texDesc.Alignment = 0;
				texDesc.MipLevels = 1;
				texDesc.DepthOrArraySize = 1;
				texDesc.Width = width;
				texDesc.Height = height;
				texDesc.SampleDesc.Count = 1;
				texDesc.SampleDesc.Quality = 0;
				texDesc.Format = format;
				texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
				texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

				D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
				uavDesc.Format = texDesc.Format;
				uavDesc.Texture2DArray.ArraySize = 1;
				uavDesc.Texture2DArray.FirstArraySlice = 0;
				uavDesc.Texture2DArray.MipSlice = 0;
				uavDesc.Texture2DArray.PlaneSlice = 0;

				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
				srvDesc.Texture2DArray.ArraySize = 1;
				srvDesc.Texture2DArray.FirstArraySlice = 0;
				srvDesc.Texture2DArray.MipLevels = 1;
				srvDesc.Texture2DArray.MostDetailedMip = 0;
				srvDesc.Texture2DArray.PlaneSlice = 0;
				srvDesc.Texture2DArray.ResourceMinLODClamp = 0;

				texture = std::make_shared<TextureD3D12>(device, texDesc, nullptr, QueueId::Direct);
				texture->CreateUAV(&uavDesc);
				if (createSrv)
					texture->CreateSRV(&srvDesc);

				texture->SetName(name);
			};

		CreateTexture(m_TransmittanceLut, transmittanceLUTWidth, transmittanceLUTHeight, DXGI_FORMAT_R32G32B32A32_FLOAT, L"TransmittanceLut", true);
		CreateTexture(m_MultiscatteringLUT, multiscatteringLUTWidth, multiscatteringLUTHeight, DXGI_FORMAT_R32G32B32A32_FLOAT, L"MultiscatteringLUT", true);
		CreateTexture(m_SkyViewLUT, skyViewLUTWidth, skyViewLUTHeight, DXGI_FORMAT_R32G32B32A32_FLOAT, L"SkyViewLUT", true);

		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(device);

		D3D12_RESOURCE_DESC buffDesc = {};
		buffDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		buffDesc.Width = sizeof(AtmosphereParams);
		buffDesc.Alignment = 0;
		buffDesc.Height = 1;
		buffDesc.DepthOrArraySize = 1;
		buffDesc.MipLevels = 1;
		buffDesc.SampleDesc.Count = 1;
		buffDesc.SampleDesc.Quality = 0;
		buffDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		buffDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		m_AtmosphereParamsBuffer = std::make_shared<BufferD3D12>(device, context, buffDesc, QueueId::Direct);
		m_AtmosphereParamsBuffer->SetName(L"AtmosphereParamsBuffer");

		buffDesc.Width = sizeof(LUTSizes);
		m_LUTSizeBuffer = std::make_shared<BufferD3D12>(device, context, buffDesc, QueueId::Direct);
		m_LUTSizeBuffer->SetName(L"LUTSizeBuffer");

		m_TransmittanceLUTBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbLUTSizes", m_LUTSizeBuffer);
		m_TransmittanceLUTBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbAtmosphereParameters", m_AtmosphereParamsBuffer);
		m_TransmittanceLUTBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "gTransmittanceLUT", m_TransmittanceLut);

		m_MultiscatteringLUTBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbLUTSizes", m_LUTSizeBuffer);
		m_MultiscatteringLUTBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbAtmosphereParameters", m_AtmosphereParamsBuffer);
		m_MultiscatteringLUTBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "gTransmittanceLUT", m_TransmittanceLut);
		m_MultiscatteringLUTBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "gMultiscatteringLUT", m_MultiscatteringLUT);

		m_SkyViewLUTBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbLUTSizes", m_LUTSizeBuffer);
		m_SkyViewLUTBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbAtmosphereParameters", m_AtmosphereParamsBuffer);
		m_SkyViewLUTBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "gTransmittanceLUT", m_TransmittanceLut);
		m_SkyViewLUTBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "gMultiscatteringLUT", m_MultiscatteringLUT);
		m_SkyViewLUTBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "gSkyViewLUT", m_SkyViewLUT);
		m_SkyViewLUTBinder->BindDynamicResource(EDU_SHADER_TYPE_COMPUTE, "cbPass", m_PassBuffer);

		// Build Draw PSO
		{
			ShaderResourceDesc resDesc[]
			{
				ShaderResourceDesc("cbPass", SHADER_RESOURCE_TYPE_DYNAMIC),
			};

			ShaderDesc sDesc = {};
			sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
			sDesc.ResourceDesc = resDesc;
			sDesc.ResourceNum = _countof(resDesc);

			std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			};

			D3D12_DEPTH_STENCIL_DESC dss = {};
			dss.DepthEnable = true;
			dss.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
			dss.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;

			auto vs = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Atmosphere\\AtmosphereDraw.hlsl", L"VS", L"vs_6_6", nullptr, sDesc);
			auto ps = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Atmosphere\\AtmosphereDraw.hlsl", L"PS", L"ps_6_6", nullptr, sDesc);

			m_DrawPassBuffer = std::make_shared<DynamicUploadBuffer>(device);

			m_DrawPso.SetDepthStencilState(dss);
			m_DrawPso.SetInputLayout({ mInputLayout.data(), (UINT)mInputLayout.size() });
			m_DrawPso.SetShader(vs);
			m_DrawPso.SetShader(ps);
			m_DrawPso.SetRTVFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
			m_DrawPso.Build(device);
			m_DrawPso.SetName(L"SkyDrawPso");

			dss.DepthEnable = false;

			m_ReflectionCubePso.SetDepthStencilState(dss);
			m_ReflectionCubePso.SetInputLayout({ mInputLayout.data(), (UINT)mInputLayout.size() });
			m_ReflectionCubePso.SetShader(vs);
			m_ReflectionCubePso.SetShader(ps);
			m_ReflectionCubePso.SetRTVFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
			m_ReflectionCubePso.Build(device);
			m_ReflectionCubePso.SetName(L"ReflectionCubePso");

			m_DrawBinder = m_DrawPso.CreateShaderBinder();
			m_DrawBinder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_DrawPassBuffer);
			m_DrawBinder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_DrawPassBuffer);
			m_DrawBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "gSkyLUT", m_SkyViewLUT);
		}

		//
		// Reflection Cube
		//
		{
			D3D12_RESOURCE_DESC texDesc;
			ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
			texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			texDesc.Width = reflectionMapSize;
			texDesc.Height = reflectionMapSize;
			texDesc.Alignment = 0;
			texDesc.DepthOrArraySize = 6;
			texDesc.MipLevels = 1;
			texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			texDesc.SampleDesc.Count = 1;
			texDesc.SampleDesc.Quality = 0;
			texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			D3D12_CLEAR_VALUE optClear = {};
			optClear.Format = texDesc.Format;
			optClear.DepthStencil.Depth = 0.0f;
			optClear.DepthStencil.Stencil = 0;

			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = { };
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Format = texDesc.Format;
			rtvDesc.Texture2DArray.ArraySize = 1;

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = texDesc.Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MostDetailedMip = 0;
			srvDesc.TextureCube.MipLevels = texDesc.MipLevels;
			srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

			m_ReflectionCube = std::make_shared<TextureD3D12>(device, texDesc, &optClear, QueueId::Direct);
			m_ReflectionCube->CreateRTV_Array(rtvDesc);
			m_ReflectionCube->CreateSRV(&srvDesc, false);
			m_ReflectionCube->SetName(L"AtmosphereReflectionCube");

			context->GetCommandCtx()->TransitionResource(m_ReflectionCube.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		}

		//	TODO: Share cube buffers!
		// 
		//	Generate cube buffers
		//
		DirectX::XMFLOAT3 vertices[] =
		{
			// +X
			{ +1, +1, -1 }, { +1, -1, -1 }, { +1, -1, +1 }, { +1, +1, +1 },
			// -X
			{ -1, +1, +1 }, { -1, -1, +1 }, { -1, -1, -1 }, { -1, +1, -1 },
			// +Y
			{ -1, +1, +1 }, { -1, +1, -1 }, { +1, +1, -1 }, { +1, +1, +1 },
			// -Y
			{ -1, -1, -1 }, { -1, -1, +1 }, { +1, -1, +1 }, { +1, -1, -1 },
			// +Z
			{ -1, +1, +1 }, { +1, +1, +1 }, { +1, -1, +1 }, { -1, -1, +1 },
			// -Z
			{ +1, +1, -1 }, { -1, +1, -1 }, { -1, -1, -1 }, { +1, -1, -1 },
		};

		uint16 indices[] = {
			0,1,2,		0,2,3,		// +X
			4,5,6,		4,6,7,      // -X
			8,9,10,		8,10,11,	// +Y
			12,13,14,	12,14,15,	// -Y
			16,17,18,	16,18,19,	// +Z
			20,21,22,	20,22,23,	// -Z
		};

		m_CubeVB = std::make_shared<VertexBufferD3D12>(device, context, vertices, sizeof(DirectX::XMFLOAT3), _countof(vertices));
		m_CubeIB = std::make_shared<IndexBufferD3D12>(device, context, indices, sizeof(uint16), _countof(indices), DXGI_FORMAT_R16_UINT);

		UpdateSettings(m_Settings);
	}

	void Atmosphere::Render(const Camera* camera, XMFLOAT3 sunDirection)
	{
		m_PassBuffer->LoadData(m_Context, sunDirection);

		m_SkyViewLUTPso->BeginPSOAndCommitResources(m_Context, m_SkyViewLUTBinder.get());
		m_Context->GetCommandCtx()->GetCmdList()->Dispatch(skyViewLUTWidth / LOCAL_WORK_GROUPS_X, skyViewLUTHeight / LOCAL_WORK_GROUPS_Y, 1);

		// Compute the cosine of the angle between sun and zenith, in range: [-1, 1]
		// Range [-1, 1] is remapped to [0, 1] for sampling the color gradient.
		XMVECTOR sunDirV = XMVector3Normalize(XMVectorNegate(XMLoadFloat3(&sunDirection)));
		XMVECTOR downV = { 0, -1, 0, 0 };
		float dot = XMVectorGetX(XMVector3Dot(sunDirV, downV));
		float sunElevation = (dot + 1.0f) * 0.5f;
		m_SunColor = m_SunColorsGradient.GetColor(sunElevation);

		struct DrawData
		{
			XMFLOAT4X4 View;
			XMFLOAT4X4 Proj;
			XMFLOAT4 MainLightColor;
			XMFLOAT3 MainLightPosition;
			float SunSize;
		} drawData;

		XMStoreFloat4x4(&drawData.View, XMMatrixTranspose(XMLoadFloat4x4(&camera->GetViewMatrix())));
		XMStoreFloat4x4(&drawData.Proj, XMMatrixTranspose(XMLoadFloat4x4(&camera->GetProjectionMatrix())));
		drawData.MainLightColor = m_SunColor;
		drawData.MainLightPosition = sunDirection;
		drawData.SunSize = m_Settings.SunSize;

		m_DrawPassBuffer->LoadData(m_Context, drawData);

		m_DrawPso.BeginPSOAndCommitResources(m_Context, m_DrawBinder.get());

		m_Context->GetCommandCtx()->GetCmdList()->IASetVertexBuffers(0, 1, &m_CubeVB->GetView());
		m_Context->GetCommandCtx()->GetCmdList()->IASetIndexBuffer(&m_CubeIB->GetView());

		m_Context->GetCommandCtx()->GetCmdList()->DrawIndexedInstanced(m_CubeIB->GetLength(), 1, 0, 0, 0);

		//
		// Create reflection Cube Map
		//
		float nearValue = 0.1f;
		float farValue = 300.0f;

		static Camera faceCamera[6]
		{
			Camera(reflectionMapSize, reflectionMapSize, XM_PIDIV2, nearValue, farValue, { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }),
			Camera(reflectionMapSize, reflectionMapSize, XM_PIDIV2, nearValue, farValue, { 0, 0, 0 }, {-1, 0, 0 }, { 0, 1, 0 }),
			Camera(reflectionMapSize, reflectionMapSize, XM_PIDIV2, nearValue, farValue, { 0, 0, 0 }, { 0, 1, 0 }, { 0, 0,-1 }),
			Camera(reflectionMapSize, reflectionMapSize, XM_PIDIV2, nearValue, farValue, { 0, 0, 0 }, { 0,-1, 0 }, { 0, 0, 1 }),
			Camera(reflectionMapSize, reflectionMapSize, XM_PIDIV2, nearValue, farValue, { 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }),
			Camera(reflectionMapSize, reflectionMapSize, XM_PIDIV2, nearValue, farValue, { 0, 0, 0 }, { 0, 0,-1 }, { 0, 1, 0 }),
		};

		D3D12_VIEWPORT viewport = {};
		viewport.Width = reflectionMapSize;
		viewport.Height = reflectionMapSize;
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.MinDepth = 0;
		viewport.MaxDepth = 1;

		D3D12_RECT scissorRect = { 0, 0, (int)viewport.Width, (int)viewport.Height };
		
		CommandContext* commandContext = m_Context->GetCommandCtx();

		for (size_t i = 0; i < 6; i++)
		{
			XMStoreFloat4x4(&drawData.View, XMMatrixTranspose(XMLoadFloat4x4(&faceCamera[i].GetViewMatrix())));
			XMStoreFloat4x4(&drawData.Proj, XMMatrixTranspose(XMLoadFloat4x4(&faceCamera[i].GetProjectionMatrix())));
			m_DrawPassBuffer->LoadData(m_Context, drawData);

			commandContext->SetViewports(&viewport, 1);
			commandContext->SetScissorRects(&scissorRect, 1);

			commandContext->SetRenderTargets(1, &m_ReflectionCube->GetRTVView()->GetCpuHandle(i), false, nullptr);

			m_ReflectionCubePso.BeginPSOAndCommitResources(m_Context, m_DrawBinder.get());

			m_Context->GetCommandCtx()->GetCmdList()->IASetVertexBuffers(0, 1, &m_CubeVB->GetView());
			m_Context->GetCommandCtx()->GetCmdList()->IASetIndexBuffer(&m_CubeIB->GetView());

			m_Context->GetCommandCtx()->GetCmdList()->DrawIndexedInstanced(m_CubeIB->GetLength(), 1, 0, 0, 0);
		}
	}

	void Atmosphere::UpdateSettings(Settings newSettings)
	{
		m_Settings = newSettings;

		AtmosphereParams params = {};
		params.PlanetRadius = m_Settings.PlanetRadius;
		params.AtmosphereRadius = m_Settings.AtmosphereRadius;
		params.MieG = m_Settings.MieG;

		params.RayleighScatteringCoefficient = m_Settings.RayleighScatteringCoefficient;
		params.RayleighAbsorptionCoefficient = m_Settings.RayleighAbsorptionCoefficient;
		params.RayleighScaleHeight = m_Settings.RayleighScaleHeight;

		params.MieScatteringCoefficient = m_Settings.MieScatteringCoefficient;
		params.MieAbsorptionCoefficient = m_Settings.MieAbsorptionCoefficient;
		params.MieScaleHeight = m_Settings.MieScaleHeight;

		params.OzoneScatteringCoefficient = m_Settings.OzoneScatteringCoefficient;
		params.OzoneAbsorptionCoefficient = m_Settings.OzoneAbsorptionCoefficient;

		params.GroundSpectrumAlbedo = m_Settings.GroundSpectrumAlbedo;

		m_AtmosphereParamsBuffer->LoadData(m_Context, &params);

		// Compute transmittance
		{
			LUTSizes lutSizes = {};
			lutSizes.LutWidth = transmittanceLUTWidth;
			lutSizes.LutHeight = transmittanceLUTHeight;
			m_LUTSizeBuffer->LoadData(m_Context, &lutSizes);

			m_TransmittanceLUTPso->BeginPSOAndCommitResources(m_Context, m_TransmittanceLUTBinder.get());
			m_Context->GetCommandCtx()->GetCmdList()->Dispatch(transmittanceLUTWidth / LOCAL_WORK_GROUPS_X, transmittanceLUTHeight / LOCAL_WORK_GROUPS_Y, 1);

			m_Context->GetCommandCtx()->TransitionResource(m_TransmittanceLut.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
		}

		// TODO: Refactor
		// Read sun colors
		{
			UINT64 size = sizeof(float) * 4 * transmittanceLUTWidth * transmittanceLUTHeight;
			ReadBackBufferD3D12 sunColorsReadback(m_Device, size, QueueId::Direct);
			sunColorsReadback.SetName(L"SunColorsReadback");

			sunColorsReadback.CopyTexture(m_Context, m_TransmittanceLut.get());

			m_Device->FlushQueues();
			auto& queue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
			CommandContext* ctx[]{ m_Context->GetCommandCtx() };
			queue.CloseAndExecuteCommandContexts(ctx, 1);
			m_Context->FinishFrame();
			m_Device->FlushQueues();

			ID3D12DescriptorHeap* descriptorHeaps[] = { m_Device->GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };
			m_Context->GetCommandCtx()->GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

			uint32 pixelSize = sizeof(float) * 4;
			uint32 rowPitch = pixelSize * transmittanceLUTWidth;

			float intervals[8] = { 0.01f, 0.14f, 0.28f, 0.36f, 0.57f, 0.75f, 0.86f, 0.99f };
			XMFLOAT4 pixels[8];

			for (size_t i = 0; i < 8; i++)
				sunColorsReadback.ReadData<XMFLOAT4>(&pixels[i], pixelSize, (int)(intervals[i] * transmittanceLUTHeight) * rowPitch);

			m_SunColorsGradient.SetColors(pixels);
		}

		// Compute multiscattering
		{
			LUTSizes lutSizes = {};
			lutSizes.LutWidth = multiscatteringLUTWidth;
			lutSizes.LutHeight = multiscatteringLUTHeight;
			m_LUTSizeBuffer->LoadData(m_Context, &lutSizes);

			m_MultiscatteringLUTPso->BeginPSOAndCommitResources(m_Context, m_MultiscatteringLUTBinder.get());
			m_Context->GetCommandCtx()->GetCmdList()->Dispatch(multiscatteringLUTWidth / LOCAL_WORK_GROUPS_X, multiscatteringLUTHeight / LOCAL_WORK_GROUPS_Y, 1);

			m_Context->GetCommandCtx()->TransitionResource(m_TransmittanceLut.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			m_Context->GetCommandCtx()->TransitionResource(m_MultiscatteringLUT.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
		}

		// Setup sky view LUT sizes
		{
			LUTSizes lutSizes = {};
			lutSizes.LutWidth = skyViewLUTWidth;
			lutSizes.LutHeight = skyViewLUTHeight;
			m_LUTSizeBuffer->LoadData(m_Context, &lutSizes);
		}
	}
}