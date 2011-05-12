#include "DXUT.h"
#include "ModelRenderer.h"

ModelRenderer::ModelRenderer()
	: _meshVertexShader(NULL), _meshPixelShader(NULL), _depthVertexShader(NULL), _depthPixelShader(NULL),
	  _meshInputLayout(NULL)
{
}

ModelRenderer::~ModelRenderer()
{
}

HRESULT ModelRenderer::RenderDepth(ID3D11DeviceContext* pd3dDeviceContext, vector<ModelInstance*> instances, Camera* camera)
{
	HRESULT hr;
	D3D11_MAPPED_SUBRESOURCE mappedResource;	
	
	D3DXMATRIX viewProj;
	D3DXMatrixMultiply(&viewProj, camera->GetView(), camera->GetProjection());

	BoundingFrustum cameraFrust = BoundingFrustum(viewProj);

	pd3dDeviceContext->GSSetShader(NULL, NULL, 0);
	pd3dDeviceContext->VSSetShader(_depthVertexShader, NULL, 0);
	pd3dDeviceContext->PSSetShader(_depthPixelShader, NULL, 0);	

	pd3dDeviceContext->IASetInputLayout(_depthInputLayout);
	pd3dDeviceContext->OMSetDepthStencilState(_dsStates.GetDepthWriteEnabled(), 0);

	float blendFactor[4] = {1, 1, 1, 1};
	pd3dDeviceContext->OMSetBlendState(_blendStates.GetBlendDisabled(), blendFactor, 0xFFFFFFFF);
	
	for (vector<ModelInstance*>::iterator i = instances.begin(); i != instances.end(); i++)
	{
		// Skip this model if it's bounding box is not in the frustum
		if (!Intersection::Contains(&cameraFrust, (*i)->GetBounds()))
		{
			continue;
		}

		D3DXMATRIX world = *(*i)->GetWorld();

		D3DXMATRIX wvp;
		D3DXMatrixMultiply(&wvp, &world, &viewProj);

		D3DXMatrixTranspose(&world, &world);
		D3DXMatrixTranspose(&wvp, &wvp);

		V(pd3dDeviceContext->Map(_constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
		CB_MODEL_PROPERTIES* modelProperties = (CB_MODEL_PROPERTIES*)mappedResource.pData;
		modelProperties->World = world;
		modelProperties->WorldViewProjection = wvp;
		pd3dDeviceContext->Unmap(_constantBuffer, 0);

		pd3dDeviceContext->VSSetConstantBuffers(0, 1, &_constantBuffer);

		CDXUTSDKMesh* mesh = (*i)->GetMesh();
		mesh->Render(pd3dDeviceContext);
	}

	return S_OK;
}

HRESULT ModelRenderer::RenderModels(ID3D11DeviceContext* pd3dDeviceContext, vector<ModelInstance*> instances, Camera* camera)
{
	HRESULT hr;
	D3D11_MAPPED_SUBRESOURCE mappedResource;	

	D3DXMATRIX viewProj;
	D3DXMatrixMultiply(&viewProj, camera->GetView(), camera->GetProjection());

	BoundingFrustum cameraFrust = BoundingFrustum(viewProj);

	pd3dDeviceContext->GSSetShader(NULL, NULL, 0);
	pd3dDeviceContext->VSSetShader(_meshVertexShader, NULL, 0);
	pd3dDeviceContext->PSSetShader(_meshPixelShader, NULL, 0);	

	pd3dDeviceContext->IASetInputLayout(_meshInputLayout);
	pd3dDeviceContext->OMSetDepthStencilState(_dsStates.GetDepthWriteEnabled(), 0);

	float blendFactor[4] = {1, 1, 1, 1};
	pd3dDeviceContext->OMSetBlendState(_blendStates.GetAlphaBlend(), blendFactor, 0xFFFFFFFF);

	ID3D11SamplerState* sampler = _samplerStates.GetLinear();
	pd3dDeviceContext->PSSetSamplers(0, 1, &sampler);

	for (vector<ModelInstance*>::iterator i = instances.begin(); i != instances.end(); i++)
	{
		// Skip this model if it's bounding box is not in the frustum
		if (!Intersection::Contains(&cameraFrust, (*i)->GetBounds()))
		{
			continue;
		}

		D3DXMATRIX world = *(*i)->GetWorld();

		D3DXMATRIX wvp;
		D3DXMatrixMultiply(&wvp, &world, &viewProj);

		D3DXMatrixTranspose(&world, &world);
		D3DXMatrixTranspose(&wvp, &wvp);

		V(pd3dDeviceContext->Map(_constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
		CB_MODEL_PROPERTIES* modelProperties = (CB_MODEL_PROPERTIES*)mappedResource.pData;
		modelProperties->World = world;
		modelProperties->WorldViewProjection = wvp;
		pd3dDeviceContext->Unmap(_constantBuffer, 0);

		pd3dDeviceContext->VSSetConstantBuffers(0, 1, &_constantBuffer);

		CDXUTSDKMesh* mesh = (*i)->GetMesh();
		mesh->Render(pd3dDeviceContext, 0, 1, 2);
	}

	return S_OK;
}

HRESULT ModelRenderer::OnD3D11CreateDevice(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
	HRESULT hr;
	ID3DBlob* pBlob = NULL;

	V_RETURN( CompileShaderFromFile( L"Mesh.hlsl", "PS_Model", "ps_4_0", &pBlob ) );   
    V_RETURN( pd3dDevice->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &_meshPixelShader));
	SAFE_RELEASE(pBlob);	

	V_RETURN( CompileShaderFromFile( L"Mesh.hlsl", "VS_Model", "vs_4_0", &pBlob ) );   
    V_RETURN( pd3dDevice->CreateVertexShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &_meshVertexShader));
	
	const D3D11_INPUT_ELEMENT_DESC layout_mesh[] =
    {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

	V_RETURN( pd3dDevice->CreateInputLayout(layout_mesh, ARRAYSIZE(layout_mesh), pBlob->GetBufferPointer(),
		pBlob->GetBufferSize(), &_meshInputLayout));
	SAFE_RELEASE(pBlob);

	V_RETURN( CompileShaderFromFile( L"Mesh.hlsl", "PS_ModelDepth", "ps_4_0", &pBlob ) );   
	V_RETURN( pd3dDevice->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &_depthPixelShader));
	SAFE_RELEASE(pBlob);	

	V_RETURN( CompileShaderFromFile( L"Mesh.hlsl", "VS_ModelDepth", "vs_4_0", &pBlob ) );   
	V_RETURN( pd3dDevice->CreateVertexShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &_depthVertexShader));

	const D3D11_INPUT_ELEMENT_DESC layout_depth[] =
	{
		{ "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	V_RETURN( pd3dDevice->CreateInputLayout(layout_depth, ARRAYSIZE(layout_depth), pBlob->GetBufferPointer(),
		pBlob->GetBufferSize(), &_depthInputLayout));
	SAFE_RELEASE(pBlob);

	D3D11_BUFFER_DESC bufferDesc =
	{
		sizeof(CB_MODEL_PROPERTIES), //UINT ByteWidth;
		D3D11_USAGE_DYNAMIC, //D3D11_USAGE Usage;
		D3D11_BIND_CONSTANT_BUFFER, //UINT BindFlags;
		D3D11_CPU_ACCESS_WRITE, //UINT CPUAccessFlags;
		0, //UINT MiscFlags;
		0, //UINT StructureByteStride;
	};

	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, NULL, &_constantBuffer));
	
	V_RETURN(_dsStates.OnD3D11CreateDevice(pd3dDevice, pBackBufferSurfaceDesc));
	V_RETURN(_samplerStates.OnD3D11CreateDevice(pd3dDevice, pBackBufferSurfaceDesc));
	V_RETURN(_blendStates.OnD3D11CreateDevice(pd3dDevice, pBackBufferSurfaceDesc));
	V_RETURN(_rasterStates.OnD3D11CreateDevice(pd3dDevice, pBackBufferSurfaceDesc));

	return S_OK;
}

void ModelRenderer::OnD3D11DestroyDevice()
{
	SAFE_RELEASE(_meshVertexShader);
	SAFE_RELEASE(_meshPixelShader);
	SAFE_RELEASE(_meshInputLayout);

	SAFE_RELEASE(_depthVertexShader);
	SAFE_RELEASE(_depthPixelShader);
	SAFE_RELEASE(_depthInputLayout);

	SAFE_RELEASE(_constantBuffer);

	_dsStates.OnD3D11DestroyDevice();
	_samplerStates.OnD3D11DestroyDevice();
	_blendStates.OnD3D11DestroyDevice();
	_rasterStates.OnD3D11DestroyDevice();
}

HRESULT ModelRenderer::OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                        const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
	HRESULT hr;

	V_RETURN(_dsStates.OnD3D11ResizedSwapChain(pd3dDevice, pSwapChain, pBackBufferSurfaceDesc));
	V_RETURN(_samplerStates.OnD3D11ResizedSwapChain(pd3dDevice, pSwapChain, pBackBufferSurfaceDesc));
	V_RETURN(_blendStates.OnD3D11ResizedSwapChain(pd3dDevice, pSwapChain, pBackBufferSurfaceDesc));
	V_RETURN(_rasterStates.OnD3D11ResizedSwapChain(pd3dDevice, pSwapChain, pBackBufferSurfaceDesc));

	return S_OK;
}

void ModelRenderer::OnD3D11ReleasingSwapChain()
{
	_dsStates.OnD3D11ReleasingSwapChain();
	_samplerStates.OnD3D11ReleasingSwapChain();
	_blendStates.OnD3D11ReleasingSwapChain();
	_rasterStates.OnD3D11ReleasingSwapChain();
}