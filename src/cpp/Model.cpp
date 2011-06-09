#include "Model.h"
#include "SDKmesh.h"

Model::Model()
	: _meshes(NULL), _meshCount(0), _materials(NULL), _materialCount(0)
{
}

Model::~Model()
{
}

HRESULT Model::CreateFromSDKMeshFile(ID3D11Device* device, LPCWSTR fileName)
{
	HRESULT hr;
	CDXUTSDKMesh sdkMesh;
    sdkMesh.Create(device, fileName);
	
	// Make materials
    _materialCount = sdkMesh.GetNumMaterials();
	_materials = new Material[_materialCount];
    for (UINT i = 0; i < _materialCount; i++)
    {
        V_RETURN(_materials[i].CreateFromSDKMeshMaterial(device, &sdkMesh, i));
    }
	
	// Copy the meshes
	_meshCount = sdkMesh.GetNumMeshes();
	_meshes = new Mesh[_meshCount];
    for (UINT i = 0; i < _meshCount; i++)
	{
		V_RETURN(_meshes[i].CreateFromSDKMeshMesh(device, &sdkMesh, i));
	}

	// Done with the sdk mesh, free all it's resources
	sdkMesh.Destroy();

	// Compute the overall bounding box
	if (_meshCount > 0)
	{
		_boundingBox = _meshes[0].GetAxisAlignedBox();

		for (UINT i = 1; i < _meshCount; i++)
		{
			AxisAlignedBox aaBox = _meshes[i].GetAxisAlignedBox();

			Collision::MergeAxisAlignedBoxes(&_boundingBox, &_boundingBox, &aaBox);
		}
	}

	return S_OK;
}

void Model::Destroy()
{
	for (UINT i = 0; i < _materialCount; i++)
	{
		_materials[i].Destroy();
	}
	SAFE_DELETE_ARRAY(_materials);
	_materialCount = 0;

	for (UINT i = 0; i < _meshCount; i++)
    {
		_meshes[i].Destroy();
	}
	SAFE_DELETE_ARRAY(_meshes);
	_meshCount = 0;
}

HRESULT Model::Render(ID3D11DeviceContext* context,  UINT materialBufferSlot, UINT diffuseSlot,
	UINT normalSlot, UINT specularSlot)
{
	HRESULT hr;

	for (UINT i = 0; i < _meshCount; i++)
	{
		V_RETURN(RenderMesh(context, i, materialBufferSlot, diffuseSlot, normalSlot, specularSlot));
	}

	return S_OK;
}

HRESULT Model::RenderMesh(ID3D11DeviceContext* context, UINT meshIdx, UINT materialBufferSlot,
	UINT diffuseSlot, UINT normalSlot, UINT specularSlot)
{
	context->IASetVertexBuffers(0, _meshes[meshIdx].GetVertexBufferCount(), 
		_meshes[meshIdx].GetVertexBuffers(), _meshes[meshIdx].GetVertexStrides(),
		_meshes[meshIdx].GetOffsets());
	context->IASetIndexBuffer(_meshes[meshIdx].GetIndexBuffer(), _meshes[meshIdx].GetIndexBufferFormat(), 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	for (UINT i = 0; i < _meshes[meshIdx].GetMeshPartCount(); i++)
	{
		MeshPart part = _meshes[meshIdx].GetMeshPart(i);
		UINT matIdx = part.MaterialIndex;

		if (materialBufferSlot != INVALID_BUFFER_SLOT)
		{
			ID3D11Buffer* buf = _materials[matIdx].GetPropertiesBuffer();
			context->PSSetConstantBuffers(materialBufferSlot, 1, &buf);
		}
		if (diffuseSlot != INVALID_SAMPLER_SLOT)
		{
			ID3D11ShaderResourceView* srv = _materials[matIdx].GetDiffuseSRV();
			context->PSSetShaderResources(diffuseSlot, 1, &srv);
		}
		if (normalSlot != INVALID_SAMPLER_SLOT)
		{
			ID3D11ShaderResourceView* srv = _materials[matIdx].GetNormalSRV();
			context->PSSetShaderResources(normalSlot, 1, &srv);
		}
		if (specularSlot != INVALID_SAMPLER_SLOT)
		{
			ID3D11ShaderResourceView* srv = _materials[matIdx].GetSpecularSRV();
			context->PSSetShaderResources(specularSlot, 1, &srv);
		}

        context->DrawIndexed(part.IndexCount, part.IndexStart, 0);
	}

	return S_OK;
}