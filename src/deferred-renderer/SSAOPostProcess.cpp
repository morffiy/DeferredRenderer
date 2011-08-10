#include "PCH.h"
#include "SSAOPostProcess.h"
#include "Logger.h"

const UINT SSAOPostProcess::SSAO_SAMPLE_COUNTS[NUM_SSAO_SAMPLE_COUNTS] = 
{
	SSAO_SAMPLE_COUNT_MAX / 64,
	SSAO_SAMPLE_COUNT_MAX / 32,
	SSAO_SAMPLE_COUNT_MAX / 16,
	SSAO_SAMPLE_COUNT_MAX / 8, 
	SSAO_SAMPLE_COUNT_MAX / 4, 
	SSAO_SAMPLE_COUNT_MAX / 2, 
	SSAO_SAMPLE_COUNT_MAX / 1
};

SSAOPostProcess::SSAOPostProcess()
	: _aoTexture(NULL), _aoRTV(NULL), _aoSRV(NULL), _blurTempTexture(NULL), _blurTempRTV(NULL), 
	  _blurTempSRV(NULL), _aoPropertiesBuffer(NULL), _randomTexture(NULL), _randomSRV(NULL)
{
	SetIsAdditive(false);

	for (UINT i = 0; i < NUM_SSAO_SAMPLE_COUNTS; i++)
	{
		_aoPSs[i] = NULL;
		_sampleDirectionsBuffers[i] = NULL;
	}
	
	for (UINT i = 0; i < 2; i++)
	{
		_downScaleTextures[i] = NULL;
		_downScaleRTVs[i] = NULL;
		_downScaleSRVs[i] = NULL;

		_scalePS[i] = NULL;
		_hBlurPS[i] = NULL;
		_vBlurPS[i] = NULL;
		_compositePS[i] = NULL;
	}
	
	// Initialize some parameters to default values
	SetSampleRadius(0.5f);
	SetBlurSigma(0.45f);
	SetSamplePower(4.5f);
	SetSampleCountIndex(3);
	SetHalfResolution(true);
}

HRESULT SSAOPostProcess::Render(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* src,
	ID3D11RenderTargetView* dstRTV, Camera* camera, GBuffer* gBuffer, LightBuffer* lightBuffer)
{
	BEGIN_EVENT_D3D(L"SSAO");

	HRESULT hr;
	D3D11_MAPPED_SUBRESOURCE mappedResource;

	// Save the old viewport
	D3D11_VIEWPORT vpOld[D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX];
    UINT nViewPorts = 1;
    pd3dImmediateContext->RSGetViewports(&nViewPorts, vpOld);
	
	// Prepare all the settings and map them
	V_RETURN(pd3dImmediateContext->Map(_aoPropertiesBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
	CB_AO_PROPERTIES* aoProperties = (CB_AO_PROPERTIES*)mappedResource.pData;

	XMFLOAT4X4 fViewProj = camera->GetViewProjection();
	XMMATRIX viewProj = XMLoadFloat4x4(&fViewProj);

	XMVECTOR det;
	XMMATRIX invViewProj = XMMatrixInverse(&det, viewProj);
		
	XMStoreFloat4x4(&aoProperties->ViewProjection, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&aoProperties->InverseViewProjection, XMMatrixTranspose(invViewProj));
	aoProperties->SampleRadius = _sampleRadius;
	aoProperties->BlurSigma = _blurSigma;
	aoProperties->GaussianNumerator = 1.0f / sqrt(2.0f * Pi * _blurSigma * _blurSigma);
	aoProperties->CameraNearClip = camera->GetNearClip();
	aoProperties->CameraFarClip = camera->GetFarClip();
	aoProperties->SamplePower = _samplePower;
	aoProperties->InverseSceneSize = _invSceneSize;

	pd3dImmediateContext->Unmap(_aoPropertiesBuffer, 0);

	// Set the constant buffers
	ID3D11Buffer* cbs[2] = 
	{
		_aoPropertiesBuffer,
		_sampleDirectionsBuffers[_sampleCountIndex],
	};

	pd3dImmediateContext->PSSetConstantBuffers(0, 2, cbs);

	// Set all the device states
	ID3D11SamplerState* samplers[2] =
	{
		GetSamplerStates()->GetPointClamp(),
		GetSamplerStates()->GetLinearClamp(),
	};

	pd3dImmediateContext->PSSetSamplers(0, 2, samplers);

	pd3dImmediateContext->OMSetDepthStencilState(GetDepthStencilStates()->GetDepthDisabled(), 0);

	float blendFactor[4] = {1, 1, 1, 1};
	pd3dImmediateContext->OMSetBlendState(GetBlendStates()->GetBlendDisabled(), blendFactor, 0xFFFFFFFF);
		
	Quad* fsQuad = GetFullScreenQuad();

	float resolutionMult = _halfRes ? 0.5f : 1.0f;

	D3D11_VIEWPORT vp;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.Width = vpOld[0].Width * resolutionMult;
	vp.Height = vpOld[0].Height * resolutionMult;
	vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;

	pd3dImmediateContext->RSSetViewports(1, &vp);
		
	// Render the SSAO
	BEGIN_EVENT_D3D(L"Occlusion");

	pd3dImmediateContext->OMSetRenderTargets(1, &_aoRTV, NULL);

	ID3D11ShaderResourceView* ppSRVAO[3] =
	{ 
		gBuffer->GetShaderResourceView(1),
		gBuffer->GetShaderResourceView(3),
		_randomSRV
	};
	pd3dImmediateContext->PSSetShaderResources(0, 3, ppSRVAO);	

	V_RETURN(fsQuad->Render(pd3dImmediateContext, _aoPSs[_sampleCountIndex]));

	END_EVENT_D3D(L"");

	// Down scale to 1/4
	BEGIN_EVENT_D3D(L"Down scale to 1/4");

	pd3dImmediateContext->OMSetRenderTargets(1, &_downScaleRTVs[0], NULL);

	ID3D11ShaderResourceView* ppDownScaleSRV[3] = { _aoSRV,	NULL, NULL };
	pd3dImmediateContext->PSSetShaderResources(0, 3, ppDownScaleSRV);

	vp.Width = vpOld[0].Width * 0.5f * resolutionMult;
	vp.Height = vpOld[0].Height * 0.5f * resolutionMult;
	pd3dImmediateContext->RSSetViewports(1, &vp);

	V_RETURN(fsQuad->Render(pd3dImmediateContext, _scalePS[_halfRes ? 1 : 0]));

	END_EVENT_D3D(L"");

	// Down scale to 1/8
	BEGIN_EVENT_D3D(L"Down scale to 1/8");

	pd3dImmediateContext->OMSetRenderTargets(1, &_downScaleRTVs[1], NULL);	
	pd3dImmediateContext->PSSetShaderResources(0, 1, &_downScaleSRVs[0]);

	vp.Width = vpOld[0].Width * 0.25f * resolutionMult;
	vp.Height = vpOld[0].Height * 0.25f * resolutionMult;
	pd3dImmediateContext->RSSetViewports(1, &vp);

	V_RETURN(fsQuad->Render(pd3dImmediateContext, _scalePS[_halfRes ? 1 : 0]));

	END_EVENT_D3D(L"");

	// Blur
	BEGIN_EVENT_D3D(L"Blur horizontal");
	pd3dImmediateContext->OMSetRenderTargets(1, &_blurTempRTV, NULL);
	pd3dImmediateContext->PSSetShaderResources(0, 1, &_downScaleSRVs[1]);
	V_RETURN(fsQuad->Render(pd3dImmediateContext, _hBlurPS[_halfRes ? 1 : 0]));
	END_EVENT_D3D(L"");

	ID3D11ShaderResourceView* ppSRVNULL1[1] = { NULL };
	pd3dImmediateContext->PSSetShaderResources(0, 1, ppSRVNULL1);

	BEGIN_EVENT_D3D(L"Blur vertical");
	pd3dImmediateContext->OMSetRenderTargets(1, &_downScaleRTVs[1], NULL);
	pd3dImmediateContext->PSSetShaderResources(0, 1, &_blurTempSRV);
	V_RETURN(fsQuad->Render(pd3dImmediateContext, _vBlurPS[_halfRes ? 1 : 0]));
	END_EVENT_D3D(L"");
	
	// Upscale to 1/4
	BEGIN_EVENT_D3D(L"Upscale to 1/4");

	pd3dImmediateContext->OMSetRenderTargets(1, &_downScaleRTVs[0], NULL);	
	pd3dImmediateContext->PSSetShaderResources(0, 1, &_downScaleSRVs[1]);

	vp.Width = vpOld[0].Width * 0.5f * resolutionMult;
	vp.Height = vpOld[0].Height * 0.5f * resolutionMult;	
	pd3dImmediateContext->RSSetViewports(1, &vp);

	V_RETURN(fsQuad->Render(pd3dImmediateContext, _scalePS[_halfRes ? 1 : 0]));

	END_EVENT_D3D(L"");

	// Upscale to 1/2
	BEGIN_EVENT_D3D(L"Upscale to 1/2");

	pd3dImmediateContext->OMSetRenderTargets(1, &_aoRTV, NULL);	
	pd3dImmediateContext->PSSetShaderResources(0, 1, &_downScaleSRVs[0]);

	vp.Width = vpOld[0].Width * resolutionMult;
	vp.Height = vpOld[0].Height * resolutionMult;	
	pd3dImmediateContext->RSSetViewports(1, &vp);

	V_RETURN(fsQuad->Render(pd3dImmediateContext, _scalePS[_halfRes ? 1 : 0]));

	END_EVENT_D3D(L"");

	// Re-apply the old viewport
	pd3dImmediateContext->RSSetViewports(nViewPorts, vpOld);

	// Composite
	BEGIN_EVENT_D3D(L"Composite");

	pd3dImmediateContext->OMSetRenderTargets(1, &dstRTV, NULL);

	ID3D11ShaderResourceView* ppSRVToneMap[2] = { src, _aoSRV };
	pd3dImmediateContext->PSSetShaderResources(0, 2, ppSRVToneMap);

	V_RETURN(fsQuad->Render(pd3dImmediateContext, _compositePS[_halfRes ? 1 : 0]));

	END_EVENT_D3D(L"");

	// Unset the SRVs
	ID3D11ShaderResourceView* ppSRVNULL2[2] = { NULL, NULL };
	pd3dImmediateContext->PSSetShaderResources(0, 2, ppSRVNULL2);

	END_EVENT_D3D(L"");

	return S_OK;
}

HRESULT SSAOPostProcess::OnD3D11CreateDevice(ID3D11Device* pd3dDevice,
	const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
	HRESULT hr;

	V_RETURN(PostProcess::OnD3D11CreateDevice(pd3dDevice, pBackBufferSurfaceDesc));

	// Load the shaders
	ID3DBlob* pBlob = NULL;
	
	D3D_SHADER_MACRO sampleCountMacros[] = 
	{
		{ "SSAO_SAMPLE_COUNT", "" },
		NULL,
	};

	char sampleCountString[6];
	char aoPSDebugName[256];
	for (UINT i = 0; i < NUM_SSAO_SAMPLE_COUNTS; i++)
	{
		sprintf_s(sampleCountString, "%i", SSAO_SAMPLE_COUNTS[i]);
		sampleCountMacros[0].Definition = sampleCountString;

		V_RETURN( CompileShaderFromFile( L"SSAO.hlsl", "PS_SSAO", "ps_4_0", sampleCountMacros, &pBlob ) );   
		V_RETURN( pd3dDevice->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &_aoPSs[i]));
		SAFE_RELEASE(pBlob);

		sprintf_s(aoPSDebugName, "SSAO AO (sample count = %s) PS", sampleCountString);
		SET_DEBUG_NAME(_aoPSs[i], aoPSDebugName);
	}

	D3D_SHADER_MACRO halfResMacros[] = 
	{
		{ "SSAO_HALF_RES", "" },
		NULL,
	};

	// full res shaders
	halfResMacros[0].Definition = "0";
	V_RETURN( CompileShaderFromFile( L"SSAO.hlsl", "PS_Scale", "ps_4_0", halfResMacros, &pBlob ) );   
    V_RETURN( pd3dDevice->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &_scalePS[0]));
	SAFE_RELEASE(pBlob);
	SET_DEBUG_NAME(_scalePS[0], "SSAO scale (full resolution) PS");

	V_RETURN( CompileShaderFromFile( L"SSAO.hlsl", "PS_BlurHorizontal", "ps_4_0", halfResMacros, &pBlob ) );   
    V_RETURN( pd3dDevice->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &_hBlurPS[0]));
	SAFE_RELEASE(pBlob);
	SET_DEBUG_NAME(_hBlurPS[0], "SSAO horizontal blur (full resolution) PS");

	V_RETURN( CompileShaderFromFile( L"SSAO.hlsl", "PS_BlurVertical", "ps_4_0", halfResMacros, &pBlob ) );   
    V_RETURN( pd3dDevice->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &_vBlurPS[0]));
	SAFE_RELEASE(pBlob);
	SET_DEBUG_NAME(_vBlurPS[0], "SSAO vertical blur (full resolution) PS");

	V_RETURN( CompileShaderFromFile( L"SSAO.hlsl", "PS_SSAO_Composite", "ps_4_0", halfResMacros, &pBlob ) );   
    V_RETURN( pd3dDevice->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &_compositePS[0]));
	SAFE_RELEASE(pBlob);
	SET_DEBUG_NAME(_compositePS[0], "SSAO composite (full resolution) PS");

	// half res shaders
	halfResMacros[0].Definition = "1";
	V_RETURN( CompileShaderFromFile( L"SSAO.hlsl", "PS_Scale", "ps_4_0", halfResMacros, &pBlob ) );   
    V_RETURN( pd3dDevice->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &_scalePS[1]));
	SAFE_RELEASE(pBlob);
	SET_DEBUG_NAME(_scalePS[1], "SSAO scale (half resolution) PS");

	V_RETURN( CompileShaderFromFile( L"SSAO.hlsl", "PS_BlurHorizontal", "ps_4_0", halfResMacros, &pBlob ) );   
    V_RETURN( pd3dDevice->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &_hBlurPS[1]));
	SAFE_RELEASE(pBlob);
	SET_DEBUG_NAME(_hBlurPS[1], "SSAO horizontal blur (half resolution) PS");

	V_RETURN( CompileShaderFromFile( L"SSAO.hlsl", "PS_BlurVertical", "ps_4_0", halfResMacros, &pBlob ) );   
    V_RETURN( pd3dDevice->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &_vBlurPS[1]));
	SAFE_RELEASE(pBlob);
	SET_DEBUG_NAME(_vBlurPS[1], "SSAO vertical blur (half resolution) PS");

	V_RETURN( CompileShaderFromFile( L"SSAO.hlsl", "PS_SSAO_Composite", "ps_4_0", halfResMacros, &pBlob ) );   
    V_RETURN( pd3dDevice->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &_compositePS[1]));
	SAFE_RELEASE(pBlob);
	SET_DEBUG_NAME(_compositePS[1], "SSAO composite (half resolution) PS");

	// Create the buffers
	D3D11_BUFFER_DESC bufferDesc =
	{
		0, //UINT ByteWidth;
		D3D11_USAGE_DYNAMIC, //D3D11_USAGE Usage;
		D3D11_BIND_CONSTANT_BUFFER, //UINT BindFlags;
		D3D11_CPU_ACCESS_WRITE, //UINT CPUAccessFlags;
		0, //UINT MiscFlags;
		0, //UINT StructureByteStride;
	};
	
	bufferDesc.ByteWidth = sizeof(CB_AO_PROPERTIES);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, NULL, &_aoPropertiesBuffer));
	SET_DEBUG_NAME(_aoPropertiesBuffer, "SSAO properties buffer");
	
	// Create the random sample directions
	XMFLOAT4 sampleDirections[SSAO_SAMPLE_COUNT_MAX];	
	for (UINT i = 0; i < SSAO_SAMPLE_COUNT_MAX; i++)
	{
		float randPitch = (rand() / (float)RAND_MAX) * 2.0f * Pi;
		float randYaw = (rand() / (float)RAND_MAX) * 2.0f * Pi;
		float length = ((rand() / (float)RAND_MAX) * 0.9f) + 0.1f;

		XMVECTOR unitVec = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		XMMATRIX transform = XMMatrixMultiply(
			XMMatrixRotationRollPitchYaw(randPitch, randYaw, 0.0f),
			XMMatrixScaling(length, length, length));

		XMStoreFloat4(&sampleDirections[i], XMVector3TransformCoord(unitVec, transform));
	}

	D3D11_SUBRESOURCE_DATA directionInitData;
	directionInitData.pSysMem = &sampleDirections;
	directionInitData.SysMemPitch = 0;
	directionInitData.SysMemSlicePitch = 0;

	char bufferDebugName[256];
	for (UINT i = 0; i < NUM_SSAO_SAMPLE_COUNTS; i++)
	{
		bufferDesc.ByteWidth = SSAO_SAMPLE_COUNTS[i] * sizeof(XMFLOAT4);

		V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &directionInitData, &_sampleDirectionsBuffers[i]));	
				
		sprintf_s(bufferDebugName, "SSAO sample directions buffer (samples = %u)", SSAO_SAMPLE_COUNTS[i]);
		SET_DEBUG_NAME(_sampleDirectionsBuffers[i], bufferDebugName);
	}

	// The random texture and srv are not dependent on the back buffer, create them here
	D3D11_TEXTURE2D_DESC randomTextureDesc = 
    {
        RANDOM_TEXTURE_SIZE,//UINT Width;
        RANDOM_TEXTURE_SIZE,//UINT Height;
        1,//UINT MipLevels;
        1,//UINT ArraySize;
        DXGI_FORMAT_R16G16B16A16_FLOAT,//DXGI_FORMAT Format;
        1,//DXGI_SAMPLE_DESC SampleDesc;
        0,
        D3D11_USAGE_DEFAULT,//D3D11_USAGE Usage;
        D3D11_BIND_SHADER_RESOURCE,//UINT BindFlags;
        0,//UINT CPUAccessFlags;
        0//UINT MiscFlags;    
    };
	
	XMHALF4 randomData[RANDOM_TEXTURE_SIZE * RANDOM_TEXTURE_SIZE];
	for (UINT i = 0; i < RANDOM_TEXTURE_SIZE * RANDOM_TEXTURE_SIZE; i++)
	{
		float randPitch = (rand() / (float)RAND_MAX) * 2.0f * Pi;
		float randYaw = (rand() / (float)RAND_MAX) * 2.0f * Pi;

		randomData[i] = XMHALF4(0.0f, 0.0f, 1.0f, 0.0f);
		XMVECTOR vector = XMLoadHalf4(&randomData[i]);

		XMMATRIX transform = XMMatrixRotationRollPitchYaw(randPitch, randYaw, 0.0f);

		XMStoreHalf4(&randomData[i], XMVector3TransformCoord(vector, transform));
	}

	D3D11_SUBRESOURCE_DATA randomInitData;
	randomInitData.pSysMem = &randomData;
	randomInitData.SysMemPitch = RANDOM_TEXTURE_SIZE * sizeof(XMHALF4);
	randomInitData.SysMemSlicePitch = 0;

	V_RETURN(pd3dDevice->CreateTexture2D(&randomTextureDesc, &randomInitData, &_randomTexture));
	SET_DEBUG_NAME(_randomTexture, "SSAO random texture");

	// Create the shader resource view
	D3D11_SHADER_RESOURCE_VIEW_DESC randomSRVDesc = 
    {
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        D3D11_SRV_DIMENSION_TEXTURE2D,
        0,
        0
    };
	randomSRVDesc.Texture2D.MipLevels = 1;

	V_RETURN(pd3dDevice->CreateShaderResourceView(_randomTexture, &randomSRVDesc, &_randomSRV));
	SET_DEBUG_NAME(_randomSRV, "SSAO random SRV");

	_invSceneSize.x = 1.0f / pBackBufferSurfaceDesc->Width;
	_invSceneSize.y = 1.0f / pBackBufferSurfaceDesc->Height;

	return S_OK;
}

void SSAOPostProcess::OnD3D11DestroyDevice()
{
	PostProcess::OnD3D11DestroyDevice();	
	
	SAFE_RELEASE(_randomTexture);
	SAFE_RELEASE(_randomSRV);

	for (UINT i = 0; i < NUM_SSAO_SAMPLE_COUNTS; i++)
	{
		SAFE_RELEASE(_aoPSs[i]);
		SAFE_RELEASE(_sampleDirectionsBuffers[i]);
	}

	for (UINT i = 0; i < 2; i++)
	{
		SAFE_RELEASE(_scalePS[i]);
		SAFE_RELEASE(_hBlurPS[i]);
		SAFE_RELEASE(_vBlurPS[i]);
		SAFE_RELEASE(_compositePS[i]);
	}

	SAFE_RELEASE(_aoPropertiesBuffer);
}

HRESULT SSAOPostProcess::OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice,
	IDXGISwapChain* pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
	HRESULT hr;

	V_RETURN(PostProcess::OnD3D11ResizedSwapChain(pd3dDevice, pSwapChain, pBackBufferSurfaceDesc));

	// Create the AO texture components
	D3D11_TEXTURE2D_DESC aoTextureDesc = 
    {
        pBackBufferSurfaceDesc->Width,//UINT Width;
        pBackBufferSurfaceDesc->Height,//UINT Height;
        1,//UINT MipLevels;
        1,//UINT ArraySize;
        DXGI_FORMAT_R16_FLOAT,//DXGI_FORMAT Format;
        1,//DXGI_SAMPLE_DESC SampleDesc;
        0,
        D3D11_USAGE_DEFAULT,//D3D11_USAGE Usage;
        D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE,//UINT BindFlags;
        0,//UINT CPUAccessFlags;
        0//UINT MiscFlags;    
    };

	V_RETURN(pd3dDevice->CreateTexture2D(&aoTextureDesc, NULL, &_aoTexture));
	SET_DEBUG_NAME(_aoTexture, "SSAO AO texture");

	D3D11_RENDER_TARGET_VIEW_DESC aoRTVDesc = 
	{
        DXGI_FORMAT_R16_FLOAT,
        D3D11_RTV_DIMENSION_TEXTURE2D,
        0,
        0
    };

	V_RETURN(pd3dDevice->CreateRenderTargetView(_aoTexture, &aoRTVDesc, &_aoRTV));
	SET_DEBUG_NAME(_aoRTV, "SSAO AO RTV");

	D3D11_SHADER_RESOURCE_VIEW_DESC aoSRVDesc = 
    {
        DXGI_FORMAT_R16_FLOAT,
        D3D11_SRV_DIMENSION_TEXTURE2D,
        0,
        0
    };
	aoSRVDesc.Texture2D.MipLevels = 1;

	V_RETURN(pd3dDevice->CreateShaderResourceView(_aoTexture, &aoSRVDesc, &_aoSRV));
	SET_DEBUG_NAME(_aoSRV, "SSAO AO SRV");

	// Create the downscale and blur temp components
	D3D11_TEXTURE2D_DESC downScaleTextureDesc = 
    {
        1,//UINT Width;
        1,//UINT Height;
        1,//UINT MipLevels;
        1,//UINT ArraySize;
        DXGI_FORMAT_R16_FLOAT,//DXGI_FORMAT Format;
        1,//DXGI_SAMPLE_DESC SampleDesc;
        0,
        D3D11_USAGE_DEFAULT,//D3D11_USAGE Usage;
        D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE,//UINT BindFlags;
        0,//UINT CPUAccessFlags;
        0//UINT MiscFlags;    
    };

	downScaleTextureDesc.Width = pBackBufferSurfaceDesc->Width / 2;
	downScaleTextureDesc.Height = pBackBufferSurfaceDesc->Height / 2;
	V_RETURN(pd3dDevice->CreateTexture2D(&downScaleTextureDesc, NULL, &_downScaleTextures[0]));
	SET_DEBUG_NAME(_downScaleTextures[0], "SSAO downscale 1/2 texture");
	
	downScaleTextureDesc.Width = pBackBufferSurfaceDesc->Width / 4;
	downScaleTextureDesc.Height = pBackBufferSurfaceDesc->Height / 4;
	V_RETURN(pd3dDevice->CreateTexture2D(&downScaleTextureDesc, NULL, &_downScaleTextures[1]));
	SET_DEBUG_NAME(_downScaleTextures[1], "SSAO downscale 1/4 texture");

	V_RETURN(pd3dDevice->CreateTexture2D(&downScaleTextureDesc, NULL, &_blurTempTexture));
	SET_DEBUG_NAME(_blurTempTexture, "SSAO blur temp texture");

	D3D11_RENDER_TARGET_VIEW_DESC downScaleRTVDesc = 
	{
        DXGI_FORMAT_R16_FLOAT,
        D3D11_RTV_DIMENSION_TEXTURE2D,
        0,
        0
    };

	V_RETURN(pd3dDevice->CreateRenderTargetView(_downScaleTextures[0], &downScaleRTVDesc, &_downScaleRTVs[0]));
	SET_DEBUG_NAME(_downScaleRTVs[0], "SSAO downscale 1/2 RTV");

	V_RETURN(pd3dDevice->CreateRenderTargetView(_downScaleTextures[1], &downScaleRTVDesc, &_downScaleRTVs[1]));
	SET_DEBUG_NAME(_downScaleRTVs[1], "SSAO downscale 1/4 RTV");
	
	V_RETURN(pd3dDevice->CreateRenderTargetView(_blurTempTexture, &downScaleRTVDesc, &_blurTempRTV));
	SET_DEBUG_NAME(_blurTempRTV, "SSAO blur temp RTV");

	D3D11_SHADER_RESOURCE_VIEW_DESC downScaleSRVDesc = 
    {
        DXGI_FORMAT_R16_FLOAT,
        D3D11_SRV_DIMENSION_TEXTURE2D,
        0,
        0
    };
	downScaleSRVDesc.Texture2D.MipLevels = 1;

	V_RETURN(pd3dDevice->CreateShaderResourceView(_downScaleTextures[0], &downScaleSRVDesc, &_downScaleSRVs[0]));
	SET_DEBUG_NAME(_downScaleSRVs[0], "SSAO downscale 1/2 SRV");

	V_RETURN(pd3dDevice->CreateShaderResourceView(_downScaleTextures[1], &downScaleSRVDesc, &_downScaleSRVs[1]));
	SET_DEBUG_NAME(_downScaleSRVs[1], "SSAO downscale 1/4 SRV");

	V_RETURN(pd3dDevice->CreateShaderResourceView(_blurTempTexture, &downScaleSRVDesc, &_blurTempSRV));
	SET_DEBUG_NAME(_blurTempSRV, "SSAO blur temp SRV");

	_invSceneSize.x = 1.0f / pBackBufferSurfaceDesc->Width;
	_invSceneSize.y = 1.0f / pBackBufferSurfaceDesc->Height;

	return S_OK;
}

void SSAOPostProcess::OnD3D11ReleasingSwapChain()
{
	PostProcess::OnD3D11ReleasingSwapChain();

	SAFE_RELEASE(_aoTexture);
	SAFE_RELEASE(_aoRTV);
	SAFE_RELEASE(_aoSRV);

	for (UINT i = 0; i < 2; i++)
	{
		SAFE_RELEASE(_downScaleTextures[i]);
		SAFE_RELEASE(_downScaleRTVs[i]);
		SAFE_RELEASE(_downScaleSRVs[i]);
	}

	SAFE_RELEASE(_blurTempTexture);
	SAFE_RELEASE(_blurTempRTV);
	SAFE_RELEASE(_blurTempSRV);
}