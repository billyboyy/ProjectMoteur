#pragma once
#include "framework.h"
#include "DataD3D12.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "ShaderStructures.h"
#include "d3dUtil.h"
#include "GameObject.h"

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
    void                                                BuildPSO();

    void                                                DrawRenderItems();

protected:

    XMFLOAT4							                CubePos;
    XMFLOAT4X4							                CubeRotMat;

    ComPtr<ID3D12RootSignature>                         m_rootSignature = nullptr;
    ComPtr<ID3D12DescriptorHeap>                        m_cbvHeap = nullptr;

    std::unique_ptr<UploadBuffer<PassConstants>>        PassCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>>      m_objectCB = nullptr;

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

    UINT												m_passCbvOffset = 0;

    GameObject                                          gameObject;
};