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

    BuildDescriptorHeaps();
    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildShapeGeometry();
    BuildConstantBuffers();
    BuildRenderItems();
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

    for (auto& e : m_allRitems) {
        XMMATRIX world = XMLoadFloat4x4(&e->World);

        ObjectConstants objConstants;
        XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(world));
        m_objectCB->CopyData(e->ObjCBIndex, objConstants);
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

    /*XMMATRIX rotXMat = XMMatrixRotationX(0.001f);
    XMMATRIX rotYMat = XMMatrixRotationY(0.002f);
    XMMATRIX rotZMat = XMMatrixRotationZ(0.003f);

    XMMATRIX rotMat = XMLoadFloat4x4(&CubeRotMat) * rotXMat * rotYMat * rotZMat;
    XMStoreFloat4x4(&CubeRotMat, rotMat);

    XMMATRIX translationMat = XMMatrixTranslationFromVector(XMLoadFloat4(&CubePos));
    XMMATRIX worldMat = rotMat * translationMat;
    XMStoreFloat4x4(&m_world, worldMat);

    XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&m_view, view);

    XMMATRIX world = XMLoadFloat4x4(&m_world);
    XMMATRIX proj = XMLoadFloat4x4(&m_proj);
    XMMATRIX worldViewProj = world * view * proj;

    ObjectConstants objConstants;
    XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
    m_objectCB->CopyData(0, objConstants);*/

}

void RenderWindow::Draw(const GameTimer& gt)
{
    ThrowIfFailed(m_commandAllocators->Reset());

    ThrowIfFailed(m_commandList->Reset(m_commandAllocators.Get(), m_PSO.Get()));

    m_commandList->RSSetViewports(1, &m_screenViewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    CD3DX12_RESOURCE_BARRIER resourceUsage =
        CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &resourceUsage);

    m_commandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    m_commandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    m_commandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    ID3D12DescriptorHeap* descriptorHeaps[] = { m_cbvHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    int passCbvIndex = m_passCbvOffset;
    auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
    passCbvHandle.Offset(passCbvIndex, m_cbvSrvUavDescriptorSize);
    m_commandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

    DrawRenderItems(m_commandList.Get(), m_opaqueRitems);

    CD3DX12_RESOURCE_BARRIER transitionResource =
        CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_commandList->ResourceBarrier(1, &transitionResource);

    ThrowIfFailed(m_commandList->Close());

    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    ThrowIfFailed(m_swapChain->Present(0, 0));
    m_currBackBuffer = (m_currBackBuffer + 1) % c_frameCount;

    FlushCommandQueue();
}

void RenderWindow::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems) 
{

    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

    auto objectCB = m_objectCB->Resource();

    for (size_t i = 0; i < ritems.size(); i++) 
    {
        auto ri = ritems[i];
        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        UINT cbvIndex = (UINT)m_opaqueRitems.size() + ri->ObjCBIndex;
        auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
            m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
        cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);
        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }

}

void RenderWindow::BuildDescriptorHeaps()
{
    UINT objCount = (UINT)m_opaqueRitems.size();

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

    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

    UINT ObjCount = (UINT)m_opaqueRitems.size();

    for (UINT i = 0; i < ObjCount; i++) 
    {
        D3D12_GPU_VIRTUAL_ADDRESS cbAddress = m_objectCB->Resource()->GetGPUVirtualAddress();

        cbAddress += i * objCBByteSize;

        int heapIndex = ObjCount + i;

        auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(
            m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(heapIndex, m_cbvSrvUavDescriptorSize);

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
        cbvDesc.BufferLocation = cbAddress;
        cbvDesc.SizeInBytes = objCBByteSize;

        m_d3dDevice->CreateConstantBufferView(&cbvDesc, handle);
    }

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

    m_d3dDevice->CreateConstantBufferView(
        &passCbvDesc, handle);
}

void RenderWindow::BuildRootSignature()
{
    CD3DX12_ROOT_PARAMETER slotRootParameter[2];

    CD3DX12_DESCRIPTOR_RANGE cbvTable0;
    cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE cbvTable1;
    cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

    slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
    slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);

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

void RenderWindow::BuildRenderItems() {
    auto boxRitem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&boxRitem->World, XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f));
    boxRitem->ObjCBIndex = 0;
    boxRitem->Geo = m_geometries["shapeGeo"].get();
    boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
    boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].
        StartIndexLocation;
    boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].
        BaseVertexLocation;
    m_allRitems.push_back(std::move(boxRitem));
    UINT objCBIndex = 1;
    auto leftSphereRitem = std::make_unique<RenderItem>();
    XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f);
    XMStoreFloat4x4(&leftSphereRitem->World, leftSphereWorld);
    leftSphereRitem->ObjCBIndex = objCBIndex++;
    leftSphereRitem->Geo = m_geometries["shapeGeo"].get();
    leftSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
    leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
    leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
    m_allRitems.push_back(std::move(leftSphereRitem));

    for (auto& e : m_allRitems) {
        m_opaqueRitems.push_back(e.get());
    }

}

void RenderWindow::BuildShapeGeometry() {
    CreateGeometry geoGen;

    CreateGeometry::MeshData box = geoGen.CreateBox(.5f, .5f, 1.5f, 3);
    CreateGeometry::MeshData sphere = geoGen.CreateSphere(.5f, 20, 20);

    UINT boxVertexOffset = 0;
    UINT boxIndexOffset = 0;

    UINT sphereVertexOffset = (UINT)box.Vertices.size();
    UINT sphereIndexOffset = (UINT)box.Indices32.size();

    SubmeshGeometry boxSubmesh;
    boxSubmesh.IndexCount = (UINT)box.Indices32.size();
    boxSubmesh.StartIndexLocation = boxIndexOffset;
    boxSubmesh.BaseVertexLocation = boxVertexOffset;

    SubmeshGeometry sphereSubmesh;
    sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
    sphereSubmesh.StartIndexLocation = sphereIndexOffset;
    sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

    auto totalVertexCount = box.Vertices.size() + sphere.Vertices.size();

    std::vector<Vertex> vertices(totalVertexCount);

    UINT k = 0;

    for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = box.Vertices[i].Position;
        vertices[k].Color = XMFLOAT4(DirectX::Colors::DarkGreen);
    }

    for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = sphere.Vertices[i].Position;
        vertices[k].Color = XMFLOAT4(DirectX::Colors::Crimson);
    }

    std::vector<std::uint16_t> indices;

    indices.insert(indices.end(),
        std::begin(box.GetIndices16()),
        std::end(box.GetIndices16()));

    indices.insert(indices.end(),
        std::begin(sphere.GetIndices16()),
        std::end(sphere.GetIndices16()));

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();

    geo->Name = "shapeGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(),
        vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(),
        ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(m_d3dDevice.Get(),
        m_commandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(m_d3dDevice.Get(),
        m_commandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);
    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    geo->DrawArgs["box"] = boxSubmesh;
    geo->DrawArgs["sphere"] = sphereSubmesh;

    m_geometries[geo->Name] = std::move(geo);


}

//void RenderWindow::BuildBoxGeometry()
//{
//    std::array<Vertex, 8> vertices =
//    {
//        Vertex({ XMFLOAT3(-.5f, -.5f, -.5f), XMFLOAT4(Colors::White) }),
//        Vertex({ XMFLOAT3(-.5f, +.5f, -.5f), XMFLOAT4(Colors::Black) }),
//        Vertex({ XMFLOAT3(+.5f, +.5f, -.5f), XMFLOAT4(Colors::Red) }),
//        Vertex({ XMFLOAT3(+.5f, -.5f, -.5f), XMFLOAT4(Colors::Green) }),
//        Vertex({ XMFLOAT3(-.5f, -.5f, +.5f), XMFLOAT4(Colors::Blue) }),
//        Vertex({ XMFLOAT3(-.5f, +.5f, +.5f), XMFLOAT4(Colors::Yellow) }),
//        Vertex({ XMFLOAT3(+.5f, +.5f, +.5f), XMFLOAT4(Colors::Cyan) }),
//        Vertex({ XMFLOAT3(+.5f, -.5f, +.5f), XMFLOAT4(Colors::Magenta) })
//    };
//
//    std::array<std::uint16_t, 36> indices =
//    {
//        // front face
//        1, 3, 0,
//        2, 3, 1,
//
//        // back face
//        6, 4, 7,
//        5, 4, 6,
//
//        // left face
//        5, 0, 4,
//        1, 0, 5,
//
//        // right face
//        3, 6, 7,
//        2, 6, 3,
//
//        // top face
//        1, 6, 2,
//        5, 6, 1,
//
//        // bottom face
//        7, 0, 3,
//        4, 0, 7
//    };
//
//    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
//    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);
//
//    m_boxGeo = std::make_unique<MeshGeometry>();
//    m_boxGeo->Name = "boxGeo";
//
//    ThrowIfFailed(D3DCreateBlob(vbByteSize, &m_boxGeo->VertexBufferCPU));
//    CopyMemory(m_boxGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
//
//    ThrowIfFailed(D3DCreateBlob(ibByteSize, &m_boxGeo->IndexBufferCPU));
//    CopyMemory(m_boxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);
//
//    m_boxGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(m_d3dDevice.Get(),
//        m_commandList.Get(), vertices.data(), vbByteSize, m_boxGeo->VertexBufferUploader);
//
//    m_boxGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(m_d3dDevice.Get(),
//        m_commandList.Get(), indices.data(), ibByteSize, m_boxGeo->IndexBufferUploader);
//
//    m_boxGeo->VertexByteStride = sizeof(Vertex);
//    m_boxGeo->VertexBufferByteSize = vbByteSize;
//    m_boxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
//    m_boxGeo->IndexBufferByteSize = ibByteSize;
//
//    SubmeshGeometry submesh;
//    submesh.IndexCount = (UINT)indices.size();
//    submesh.StartIndexLocation = 0;
//    submesh.BaseVertexLocation = 0;
//
//    m_boxGeo->DrawArgs["box"] = submesh;
//
//    CubePos = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
//    XMVECTOR posVec = XMLoadFloat4(&CubePos);
//
//    XMMATRIX tmpMat = XMMatrixTranslationFromVector(posVec);
//    XMStoreFloat4x4(&CubeRotMat, XMMatrixIdentity());
//    XMStoreFloat4x4(&m_world, tmpMat);
//}

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
