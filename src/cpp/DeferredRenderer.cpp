#include "DeferredRenderer.h"

DeferredRenderer::DeferredRenderer()
	: Application(L"Deferred Renderer", NULL),
      _renderer(), _camera(0.1f, 40.0f, 1.0f, 1.0f),
	  _scene(L"\\models\\tankscene\\tankscene.sdkmesh")
	  //_scene(L"\\models\\sponza\\sponzanoflag.sdkmesh")
{
	_scene.SetScale(1.0f);
	_scene.SetPosition(XMFLOAT3(0.0f, 0.0f, 0.0f));
	//_scene.SetOrientation(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f));

	_camera.SetPosition(XMFLOAT3(1.0f, 4.0f, -6.0f));
	_camera.SetRotation(XMFLOAT2(-0.1f, 0.35f));

	_aoEnabled = true;
}

DeferredRenderer::~DeferredRenderer()
{
}

void DeferredRenderer::OnPreparingDeviceSettings(DeviceManager* deviceManager)
{
	Application::OnPreparingDeviceSettings(deviceManager);

	deviceManager->SetBackBufferWidth(1360);
	deviceManager->SetBackBufferHeight(768);	
	deviceManager->SetFullScreen(false);

	deviceManager->SetVSyncEnabled(false);
}

void DeferredRenderer::OnFrameMove(double totalTime, float dt)
{
	HWND hwnd = GetHWND();
	KeyboardState kb = KeyboardState::GetState();
	MouseState mouse = MouseState::GetState(hwnd);

	if (IsActive() && mouse.IsOverWindow())
	{
		if (kb.IsKeyJustPressed(Esc))
		{
			Exit();
		}

		if (kb.IsKeyJustPressed(B))
		{
			_renderer.SetDrawBoundingObjects(!_renderer.GetDrawBoundingObjects());
		}

		if (kb.IsKeyJustPressed(F11))
		{
			SetFullScreen(!GetFullScreen());
		}

		if (kb.IsKeyJustPressed(M))
		{
			SetMaximized(!GetMaximized());
		}
	
		if (kb.IsKeyJustPressed(D1))
		{
			_aoEnabled = !_aoEnabled;
		}

		if (mouse.IsButtonDown(LeftButton))
		{
			const float mouseRotateSpeed = 0.002f;
		
			XMFLOAT2 rotation = _camera.GetRotation();
			rotation.x += mouse.GetDX() * mouseRotateSpeed;
			rotation.y += mouse.GetDY() * mouseRotateSpeed;
			_camera.SetRotation(rotation);

			// Set the mouse back one frame
			MouseState::SetCursorPosition(200, 200, hwnd);

			XMFLOAT2 moveDir = XMFLOAT2(0.0f, 0.0f);
			if (kb.IsKeyDown(W) || kb.IsKeyDown(Up))
			{
				moveDir.y++;
			}
			if (kb.IsKeyDown(S) || kb.IsKeyDown(Down))
			{
				moveDir.y--;
			}
			if (kb.IsKeyDown(A) || kb.IsKeyDown(Left))
			{
				moveDir.x--;
			}
			if (kb.IsKeyDown(D) || kb.IsKeyDown(Right))
			{
				moveDir.x++;
			}
				
			const float cameraMoveSpeed = 5.0f;
			XMFLOAT3 camPos = _camera.GetPosition();
			XMFLOAT3 camForward = _camera.GetForward();
			XMFLOAT3 camRight = _camera.GetRight();

			XMVECTOR position = XMLoadFloat3(&camPos);
			XMVECTOR forward = XMLoadFloat3(&camForward);
			XMVECTOR right = XMLoadFloat3(&camRight);

			position += ((moveDir.y * forward) + (moveDir.x * right)) * (cameraMoveSpeed * dt);

			XMStoreFloat3(&camPos, position);
			_camera.SetPosition(camPos);
		}
	}

	_hdrPP.SetTimeDelta(dt);
}

HRESULT DeferredRenderer::OnD3D11FrameRender(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext)
{
	HRESULT hr;

	V_RETURN(_renderer.Begin());

	_renderer.AddModel(&_scene);	
	
	/*PointLight greenLight = 
	{
		XMFLOAT3(3.0f, 3.0f, -4.0f),
		10.0f,
		XMFLOAT3(6.0f, 8.0f, 4.0f)
	};
	_renderer.AddLight(&greenLight, true);*/

	XMFLOAT3 sunColor = XMFLOAT3(1.0f, 0.8f, 0.5f);
	float sunIntensity = 5.0f;
	DirectionalLight sun = 
	{
		XMFLOAT3(0.5f, 0.6f, 0.5f),
		XMFLOAT3(sunColor.x * sunIntensity, sunColor.y * sunIntensity, sunColor.z * sunIntensity)
	};
	_renderer.AddLight(&sun, true);
	
	float ambientIntesity = 0.4f;
	AmbientLight ambientLight = 
	{
		XMFLOAT3(ambientIntesity, ambientIntesity, ambientIntesity)
	};
	_renderer.AddLight(&ambientLight);

	_skyPP.SetSkyColor(XMFLOAT3(0.2f, 0.5f, 1.0f));	
	_skyPP.SetSunColor(sun.Color);
	_skyPP.SetSunDirection(sun.Direction);
	_skyPP.SetSunWidth(0.05f);
	_skyPP.SetSunEnabled(true);

	if (_aoEnabled)
	{
		_renderer.AddPostProcess(&_aoPP);
	}

	_renderer.AddPostProcess(&_skyPP);
	//_renderer.AddPostProcess(&_dofPP);
	_renderer.AddPostProcess(&_hdrPP);
	//_renderer.AddPostProcess(&_aaPP);

	V_RETURN(_renderer.End(pd3dDevice, pd3dImmediateContext, &_camera));

	return S_OK;
}

HRESULT DeferredRenderer::OnD3D11CreateDevice(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
	HRESULT hr;

	V_RETURN(Application::OnD3D11CreateDevice(pd3dDevice, pBackBufferSurfaceDesc));

	V_RETURN(_renderer.OnD3D11CreateDevice(pd3dDevice, pBackBufferSurfaceDesc));	
	V_RETURN(_hdrPP.OnD3D11CreateDevice(pd3dDevice, pBackBufferSurfaceDesc));
	V_RETURN(_skyPP.OnD3D11CreateDevice(pd3dDevice, pBackBufferSurfaceDesc));
	V_RETURN(_aaPP.OnD3D11CreateDevice(pd3dDevice, pBackBufferSurfaceDesc));
	V_RETURN(_aoPP.OnD3D11CreateDevice(pd3dDevice, pBackBufferSurfaceDesc));
	V_RETURN(_dofPP.OnD3D11CreateDevice(pd3dDevice, pBackBufferSurfaceDesc));
	V_RETURN(_scene.OnD3D11CreateDevice(pd3dDevice, pBackBufferSurfaceDesc));

	return S_OK;
}

void DeferredRenderer::OnD3D11DestroyDevice()
{
	Application::OnD3D11DestroyDevice();

	_renderer.OnD3D11DestroyDevice();	
	_hdrPP.OnD3D11DestroyDevice();
	_skyPP.OnD3D11DestroyDevice();
	_aaPP.OnD3D11DestroyDevice();
	_aoPP.OnD3D11DestroyDevice();
	_dofPP.OnD3D11DestroyDevice();
	_scene.OnD3D11DestroyDevice();	
}

HRESULT DeferredRenderer::OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                        const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
	HRESULT hr;

	V_RETURN(Application::OnD3D11ResizedSwapChain(pd3dDevice, pSwapChain, pBackBufferSurfaceDesc));

	float fAspectRatio = pBackBufferSurfaceDesc->Width / (float)pBackBufferSurfaceDesc->Height;
	_camera.SetAspectRatio(fAspectRatio);
	
	V_RETURN(_renderer.OnD3D11ResizedSwapChain(pd3dDevice, pSwapChain, pBackBufferSurfaceDesc));	
	V_RETURN(_hdrPP.OnD3D11ResizedSwapChain(pd3dDevice, pSwapChain, pBackBufferSurfaceDesc));
	V_RETURN(_skyPP.OnD3D11ResizedSwapChain(pd3dDevice, pSwapChain, pBackBufferSurfaceDesc));
	V_RETURN(_aaPP.OnD3D11ResizedSwapChain(pd3dDevice, pSwapChain, pBackBufferSurfaceDesc));
	V_RETURN(_aoPP.OnD3D11ResizedSwapChain(pd3dDevice, pSwapChain, pBackBufferSurfaceDesc));
	V_RETURN(_dofPP.OnD3D11ResizedSwapChain(pd3dDevice, pSwapChain, pBackBufferSurfaceDesc));
	V_RETURN(_scene.OnD3D11ResizedSwapChain(pd3dDevice, pSwapChain, pBackBufferSurfaceDesc));

	return S_OK;
}
void DeferredRenderer::OnD3D11ReleasingSwapChain()
{
	Application::OnD3D11ReleasingSwapChain();

	_renderer.OnD3D11ReleasingSwapChain();	
	_hdrPP.OnD3D11ReleasingSwapChain();
	_skyPP.OnD3D11ReleasingSwapChain();
	_aaPP.OnD3D11ReleasingSwapChain();
	_aoPP.OnD3D11ReleasingSwapChain();
	_dofPP.OnD3D11ReleasingSwapChain();
	_scene.OnD3D11ReleasingSwapChain();
}

int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
	DeferredRenderer application;
	application.Start();
}