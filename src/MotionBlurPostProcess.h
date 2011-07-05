#pragma once

#include "Defines.h"
#include "PostProcess.h"

struct CB_MOTIONBLUR_PROPERTIES
{
	XMFLOAT4 Padding;
};

class MotionBlurPostProcess : public PostProcess
{
private:
	ID3D11Buffer* _propertiesBuffer;

public:
	MotionBlurPostProcess();
	~MotionBlurPostProcess();

	HRESULT Render(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* src,
		ID3D11RenderTargetView* dstRTV, Camera* camera, GBuffer* gBuffer, LightBuffer* lightBuffer);

	HRESULT OnD3D11CreateDevice(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);
	void OnD3D11DestroyDevice();

	HRESULT OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
		const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);
	void OnD3D11ReleasingSwapChain();
};