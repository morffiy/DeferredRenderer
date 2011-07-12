#pragma once

#include "Defines.h"
#include "xnaCollision.h"
#include "IHasContent.h"
#include "Model.h"
#include <map>

class ModelInstance : public IHasContent
{
private:
	const WCHAR* _path;
	Model _model;
	
	XMFLOAT4X4 _world;
	XMFLOAT3 _position;
	float _scale;
	XMFLOAT4 _orientation;

	OrientedBox _transformedMainOrientedBox;
	OrientedBox* _transformedMeshOrientedBoxes;

	AxisAlignedBox _transformedMainAxisBox;
	AxisAlignedBox* _transformedMeshAxisBoxes;

	bool _dirty;
	void clean();

public:
	ModelInstance(const WCHAR* path);
	~ModelInstance();

	const XMFLOAT3& GetPosition() const { return _position; }
	float GetScale() const { return _scale; }
	const XMFLOAT4& GetOrientation() const { return _orientation; }

	void SetPosition(const XMFLOAT3& pos)
	{
		_position = pos;

		_dirty = true;
	}

	void SetScale(float scale)
	{
		_scale = scale;

		_dirty = true;
	}

	void SetOrientation(const XMFLOAT4& orientation)
	{
		_orientation = orientation;

		_dirty = true;
	}

	const XMFLOAT4X4& GetWorld() 
	{ 
		if (_dirty)
		{
			clean();
		}

		return _world;
	}
	
	const AxisAlignedBox& GetMeshAxisAlignedBox(UINT meshIdx)
	{
		if (_dirty)
		{
			clean();
		}

		return _transformedMeshAxisBoxes[meshIdx];
	}

	const AxisAlignedBox& GetAxisAlignedBox() 
	{ 
		if (_dirty)
		{
			clean();
		}

		return _transformedMainAxisBox;
	}


	const OrientedBox& GetMeshOrientedBox(UINT meshIdx)
	{
		if (_dirty)
		{
			clean();
		}

		return _transformedMeshOrientedBoxes[meshIdx];
	}

	const OrientedBox& GetOrientedBox() 
	{ 
		if (_dirty)
		{
			clean();
		}

		return _transformedMainOrientedBox;
	}

	UINT GetModelMeshCount() const { return _model.GetMeshCount(); }

	Model* GetModel() { return &_model; }

	HRESULT OnD3D11CreateDevice(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);	
	void OnD3D11DestroyDevice();

	HRESULT OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                            const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);
	void OnD3D11ReleasingSwapChain();
};