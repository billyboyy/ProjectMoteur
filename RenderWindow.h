#pragma once
#include "framework.h"
#include "DataD3D12.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "ShaderStructures.h"
#include "d3dUtil.h"
#include "CreateGeometry.h"

using namespace DirectX;
using namespace DX;

struct PassConstants {
    XMFLOAT4X4                                          View;
    XMFLOAT4X4                                          InvView;
    XMFLOAT4X4                                          Proj;
    XMFLOAT4X4                                          InvProj;
    XMFLOAT4X4                                          ViewProj;
    XMFLOAT4X4                                          InvViewProj;
    XMFLOAT3                                            EyePosW;

    float                                               cbPerObjectPad1;

    XMFLOAT2                                            RenderTargetSize;
    XMFLOAT2                                            InvRenderTargetSize;

    float                                               NearZ;
    float                                               FarZ;
    float                                               TotalTime;
    float                                               DeltaTime;
};

struct RenderItem {
                                                        RenderItem() = default;
    XMFLOAT4X4                                          World = MathHelper::Identity4x4();

    UINT                                                ObjCBIndex = -1;

    MeshGeometry*                                       Geo = nullptr;

    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY                            PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT                                                IndexCount = 0;
    UINT                                                StartIndexLocation = 0;
    int                                                 BaseVertexLocation = 0;
};

struct ObjectConstants
{
    XMFLOAT4X4                                          WorldViewProj = MathHelper::Identity4x4();
};

class RenderWindow : public DataGlobal
{
public:

                                                        RenderWindow(HINSTANCE hInstance);
                                                        RenderWindow(const RenderWindow& rhs) = delete;
                                                        RenderWindow& operator=(const RenderWindow& rhs) = delete;
                                                        ~RenderWindow();

    virtual bool                                        Initialize()                override;
    virtual void                                        Update(const GameTimer& gt) override;
    virtual void                                        Draw(const GameTimer& gt)   override;
    
protected:

    virtual void                                        OnResize()                  override;

    void                                                BuildDescriptorHeaps();
    void                                                BuildConstantBuffers();
    void                                                BuildRootSignature();
    void                                                BuildShadersAndInputLayout();
    void                                                BuildBoxGeometry();
    void                                                BuildPSO();

    void                                                DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
    void                                                BuildRenderItems();
    void                                                BuildShapeGeometry();

protected:

    XMFLOAT4							                CubePos;
    XMFLOAT4X4							                CubeRotMat;

    ComPtr<ID3D12RootSignature>                         m_rootSignature = nullptr;
    ComPtr<ID3D12DescriptorHeap>                        m_cbvHeap = nullptr;

    std::unique_ptr<UploadBuffer<ObjectConstants>>      m_objectCB = nullptr;
    std::unique_ptr<UploadBuffer<PassConstants>>        PassCB = nullptr;

    std::unique_ptr<MeshGeometry>                       m_boxGeo = nullptr;

    ComPtr<ID3DBlob>                                    m_vsByteCode = nullptr;
    ComPtr<ID3DBlob>                                    m_psByteCode = nullptr;

    std::vector<D3D12_INPUT_ELEMENT_DESC>               m_inputLayout;

    ComPtr<ID3D12PipelineState>                         m_PSO = nullptr;

    XMFLOAT4X4                                          m_world = MathHelper::Identity4x4();
    XMFLOAT4X4                                          m_view = MathHelper::Identity4x4();
    XMFLOAT4X4                                          m_proj = MathHelper::Identity4x4();

    float                                               m_theta = 1.5f * XM_PI;
    float                                               m_phi = XM_PIDIV4;
    float                                               m_radius = 5.0f;

    UINT                                                m_passCbvOffset = 0;

    std::vector<RenderItem*>                            m_opaqueRitems;


    std::vector<std::unique_ptr<RenderItem>>            m_allRitems;
    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_geometries;
};