#pragma once
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "CreateGeometry.h"
#include "d3dUtil.h"
#include "ShaderStructures.h"

using Microsoft::WRL::ComPtr;

struct ObjectConstants
{
	XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

struct RenderItem {
	RenderItem() = default;
	XMFLOAT4X4 World = MathHelper::Identity4x4();

	UINT ObjCBIndex = -1;

	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	std::unique_ptr<UploadBuffer<ObjectConstants>> m_objectCB = nullptr;
	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;

	std::unique_ptr<UploadBuffer<ObjectConstants>>& GetObjectCB() 
	{
		return m_objectCB;
	}

};


class GameObject {
public:
	GameObject();
	~GameObject();

	void															Init(ComPtr<ID3D12GraphicsCommandList> cmdList, ComPtr<ID3D12Device> device);
	void															BuildRenderOpBox(ComPtr<ID3D12Device> device);
	void															BuildRenderOpCircle(ComPtr<ID3D12Device> device);
	
	const std::vector<RenderItem*>&									GetOpaqueItems();
	std::vector<std::unique_ptr<RenderItem>>&						GetAllItems();
	
	void															BuildObjectConstantBuffers(ComPtr<ID3D12Device> device, std::unique_ptr<RenderItem>* object);

private:
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>	m_geometries;
	UINT															ObjIndex = 0;

	//Stock RenderItem
	std::vector<std::unique_ptr<RenderItem>>						m_allRitems;

	std::vector<RenderItem*>										m_opaqueRitems;
	std::vector<RenderItem*>										m_transparentRitems;
	UINT															m_passCbvOffset = 0;
	std::unique_ptr<MeshGeometry>									m_boxGeo = nullptr;

};