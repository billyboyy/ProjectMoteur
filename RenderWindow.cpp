#include "RenderWindow.h"

RenderWindow::RenderWindow(HINSTANCE hInstance)
    : DataGlobal(hInstance)
{
}

RenderWindow::~RenderWindow()
{
}

bool RenderWindow::Initialize()
{
    if (!DataGlobal::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators.Get(), nullptr));

    BuildRootSignature();
    BuildShadersAndInputLayout();

    gameObject.Init(m_commandList, m_d3dDevice);
    gameObject.BuildRenderOpBox(m_d3dDevice);
    gameObject.BuildRenderOpCircle(m_d3dDevice);

    BuildDescriptorHeaps();
    BuildConstantBuffers();
    BuildPSO();

    // Execute the initialization commands.
    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();

    return true;
}

void RenderWindow::OnResize()
{
    DataGlobal::OnResize();

    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&m_proj, P);
}

void RenderWindow::Update(const GameTimer& gt)
{
    // Convert Spherical to Cartesian coordinates.
    float x = m_radius * sinf(m_phi) * cosf(m_theta);
    float z = m_radius * sinf(m_phi) * sinf(m_theta);
    float y = m_radius * cosf(m_phi);

    for (auto& e : gameObject.GetAllItems()) {
        XMMATRIX world = XMLoadFloat4x4(&e->World);

        ObjectConstants objConstants;
        XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(world));
        e->m_objectCB->CopyData(0, objConstants);
    }

    XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMMATRIX proj = XMLoadFloat4x4(&m_proj);

    XMMATRIX viewProj = XMMatrixMultiply(view, proj);

    XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
    XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
    XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

    PassConstants mMainPassCB;
    XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));

    mMainPassCB.RenderTargetSize = XMFLOAT2((float)m_clientWidth, (float)m_clientHeight);
    mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / m_clientWidth, 1.0f / m_clientHeight);
    mMainPassCB.NearZ = 1.0f;
    mMainPassCB.FarZ = 1000.0f;
    mMainPassCB.TotalTime = gt.TotalTime();
    mMainPassCB.DeltaTime = gt.DeltaTime();

    PassCB->CopyData(0, mMainPassCB);
}

void RenderWindow::Draw(const GameTimer& gt)
{
    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(m_commandAllocators->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators.Get(), m_PSO.Get()));

    m_commandList->RSSetViewports(1, &m_screenViewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Indicate a state transition on the resource usage.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Clear the back buffer and depth buffer.
    m_commandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    m_commandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    m_commandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    ID3D12DescriptorHeap* descriptorHeaps[] = { m_cbvHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    m_commandList->SetGraphicsRootConstantBufferView(1, PassCB->Resource()->GetGPUVirtualAddress());

    DrawRenderItems();

    // Indicate a state transition on the resource usage.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(m_commandList->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // swap the back and front buffers
    ThrowIfFailed(m_swapChain->Present(0, 0));
    m_currBackBuffer = (m_currBackBuffer + 1) % c_frameCount;

    // Wait until frame commands are complete.  This waiting is inefficient and is
    // done for simplicity.  Later we will show how to organize our rendering code
    // so we do not have to wait per frame.
    FlushCommandQueue();
}

void RenderWindow::DrawRenderItems() 
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

    for (size_t i = 0; i < gameObject.GetOpaqueItems().size(); i++) 
    {
        auto ri = gameObject.GetOpaqueItems()[i];
    
        m_commandList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        m_commandList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        m_commandList->IASetPrimitiveTopology(ri->PrimitiveType);

        m_commandList->SetGraphicsRootConstantBufferView(0, ri->m_objectCB->Resource()->GetGPUVirtualAddress());
        m_commandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

void RenderWindow::BuildDescriptorHeaps()
{
    UINT objCount = (UINT)gameObject.GetOpaqueItems().size();

    UINT numDescriptor = (objCount + 1);
    m_passCbvOffset = objCount;

    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = numDescriptor;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;
    ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
        IID_PPV_ARGS(&m_cbvHeap)));
}

void RenderWindow::BuildConstantBuffers()
{
    m_objectCB = std::make_unique<UploadBuffer<ObjectConstants>>(m_d3dDevice.Get(), 1, true);
    PassCB = std::make_unique<UploadBuffer<PassConstants>>(m_d3dDevice.Get(), 1, true);

    UINT passObjCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

    D3D12_GPU_VIRTUAL_ADDRESS passCbAddress = PassCB->Resource()->GetGPUVirtualAddress();

    int heapIndex = m_passCbvOffset;

    auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
        m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
    handle.Offset(heapIndex, m_cbvSrvUavDescriptorSize);

    D3D12_CONSTANT_BUFFER_VIEW_DESC passCbvDesc;
    passCbvDesc.BufferLocation = passCbAddress;
    passCbvDesc.SizeInBytes = passObjCBByteSize;

    m_d3dDevice->CreateConstantBufferView(&passCbvDesc, handle);
}

void RenderWindow::BuildRootSignature()
{
    CD3DX12_ROOT_PARAMETER slotRootParameter[2];

    slotRootParameter[0].InitAsConstantBufferView(0);
    slotRootParameter[1].InitAsConstantBufferView(1);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(m_d3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignature)));
}

void RenderWindow::BuildShadersAndInputLayout()
{
    HRESULT hr = S_OK;

    m_vsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
    m_psByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");

    m_inputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void RenderWindow::BuildPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS =
    {
        reinterpret_cast<BYTE*>(m_vsByteCode->GetBufferPointer()),
        m_vsByteCode->GetBufferSize()
    };
    psoDesc.PS =
    {
        reinterpret_cast<BYTE*>(m_psByteCode->GetBufferPointer()),
        m_psByteCode->GetBufferSize()
    };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = m_backBufferFormat;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat = m_depthStencilFormat;
    ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PSO)));
}
