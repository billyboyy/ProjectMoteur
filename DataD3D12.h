#pragma once
#include "framework.h"
#include "GameTimer.h"
#include "d3dUtil.h"

using namespace Microsoft::WRL;
using namespace DX;

class DataGlobal
{
public:
	
	virtual LRESULT						MsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	HWND								MainWnd()const	{	return mhMainWnd;	}

	int									run();
	virtual bool						Initialize();

	float								AspectRatio()				const	{	return static_cast<float>(m_clientWidth) / m_clientHeight; }

protected:

										DataGlobal(HINSTANCE hInstance);
										DataGlobal(const DataGlobal& rhs) = delete;
										DataGlobal& operator=(const DataGlobal& rhs) = delete;
	virtual								~DataGlobal();
	bool								InitMainWindow();

public:

	static								DataGlobal* GetApp();

	HINSTANCE							AppInst()const;

protected:

	virtual void						Update(const GameTimer& gt) = 0;
	virtual void						Draw(const GameTimer& gt) = 0;
	bool								InitDriect3D();
	virtual void						OnResize();
	void								CreateCommandObjects();
	void								CreateSwapChain();
	virtual void						CreateRtvAndDsvDescriptorHeaps();
	void								FlushCommandQueue();

protected:

	ID3D12Resource*						CurrentBackBuffer()			const	{	return m_swapChainBuffer[m_currBackBuffer].Get();	}

	D3D12_CPU_DESCRIPTOR_HANDLE			CurrentBackBufferView()		const	{	return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),m_currBackBuffer,	m_rtvDescriptorSize);	}
	D3D12_CPU_DESCRIPTOR_HANDLE			DepthStencilView()			const	{	return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();	}

protected:

	bool								m4xMsaaState = false;
	UINT								m4xMsaaQuality = 0;

	GameTimer							m_timer;

	ComPtr<IDXGIFactory4>				m_dxgiFactory;
	ComPtr<IDXGISwapChain>				m_swapChain;
	ComPtr<ID3D12Device>				m_d3dDevice;

	ComPtr<ID3D12Fence>					m_fence;
	UINT64								m_currentFence = 0;

	ComPtr<ID3D12CommandQueue>			m_commandQueue;
	ComPtr<ID3D12CommandAllocator>		m_commandAllocators;
	ComPtr<ID3D12GraphicsCommandList>	m_commandList;

	static const UINT					c_frameCount = 2;
	int									m_currBackBuffer = 0;

	ComPtr<ID3D12Resource>				m_swapChainBuffer[c_frameCount];
	ComPtr<ID3D12Resource>				m_depthStencil;

	ComPtr<ID3D12DescriptorHeap>		m_rtvHeap;
	ComPtr<ID3D12DescriptorHeap>		m_dsvHeap;

	D3D12_VIEWPORT						m_screenViewport;
	D3D12_RECT							m_scissorRect;

	UINT								m_rtvDescriptorSize = 0;
	UINT								m_dsvDescriptorSize = 0;
	UINT								m_cbvSrvUavDescriptorSize = 0;

	std::wstring						m_mainWndCaption = L"MainWindow Cube";
	D3D_DRIVER_TYPE						m_d3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT							m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT							m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	int									m_clientWidth = 800;
	int									m_clientHeight = 600;
	
protected:

	static DataGlobal*					mApp;
	HINSTANCE							mhAppInst = nullptr; // application instance handle
	HWND								mhMainWnd = nullptr; // main window handle
	bool								mAppPaused = false;  // is the application paused?
	bool								mMinimized = false;  // is the application minimized?
	bool								mMaximized = false;  // is the application maximized?
	bool								mResizing = false;   // are the resize bars being dragged?
	bool								mFullscreenState = false;// fullscreen enabled
};