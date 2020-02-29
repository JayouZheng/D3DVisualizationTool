//
// AppEntry.h
//

#pragma once

#include "Common/DeviceResources.h"
#include "Common/TimerManager.h"
#include "AppGUI.h"
#include "Common/GeometryManager.h"
#include "Common/FrameResource.h"
#include "Common/Camera.h"

using namespace DX;
using namespace DX::GeometryManager;
using namespace DX::TimerManager;

// A basic game implementation that creates a D3D12 device and
// provides a game loop.
class AppEntry : public IDeviceNotify
{
public:

    AppEntry() noexcept(false);
    ~AppEntry();

    // Initialization and management
    void Initialize(HWND window, int width, int height);

    // Basic game loop
    void Tick();

    // IDeviceNotify
    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;

	virtual void RenderGUI() override;
	virtual void OnOptionsChanged() override;

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowMoved();
    void OnWindowSizeChanged(int width, int height);

	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);
	void OnMouseWheel(int d, WPARAM btnState, int x, int y);

	void OnKeyDown(WPARAM keyState);
	void OnKeyUp(WPARAM keyState);

	void OnKeyboardInput(const StepTimer& timer);

	LRESULT AppMessageProcessor(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Properties
    void  GetDefaultSize( int& width, int& height ) const;
	void  GetCurrentSize( int& width, int& height ) const;

	float AspectRatio() const;

private:

    void Update(const StepTimer& timer);
    void Render();

	void DrawRenderItem(std::vector<RenderItem*>& renderItemLayer);

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

	// Build Resources.
	void BuildDescriptorHeaps();
	void BuildConstantBuffers();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildLineGridGeometry();
	void BuildFSceneRenderItems(const FSceneDataSet* currentFSceneDataSet);
	void BuildPSO();

	// GUI Messages
	bool CheckInBlockAreas(int x, int y);

    // Device resources.
    std::unique_ptr<DeviceResources>			   m_deviceResources = nullptr;

	// DescHeap.
	ComPtr<ID3D12DescriptorHeap> m_srvCbvDescHeap = nullptr;
	ComPtr<ID3D12RootSignature>  m_rootSignature  = nullptr;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_PSOs;

	// Shader ByteCode.
	std::unordered_map<std::string, ComPtr<ID3DBlob>> m_shaderByteCode;

	std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

	// Device Options.
	DXGI_FORMAT m_backBufferFormat   = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; // DXGI_FORMAT_D32_FLOAT
	/// BackBufferCount Or NUM_FRAMES_IN_FLIGHT.
	UINT		m_numFrameResources  = 3; // About 2 or 3 should be Enough.

	// Geometry Data.
	std::vector<std::unique_ptr<RenderItem>> m_allRitems;
	// Render items divided by PSO.
	std::vector<RenderItem*> m_renderItemLayer[RenderLayer::Count];

	// FrameResource.
	std::unique_ptr<FrameResource> m_frameResource = nullptr;

	// GUI.
	std::unique_ptr<AppGUI>		 m_appGui = nullptr;

    // Rendering loop timer.
	StepTimer					 m_timer;

	// Camera.
	Camera m_camera;

	// Client.
	HWND m_window = nullptr;
	int  m_width  = 1280;
	int  m_height = 720;

	XMINT2 m_lastMousePos;
	bool   m_bMouseDownInBlockArea = false;

	// Others.
	std::vector<StructureBuffer> m_perFSceneCPUSBuffer;
	std::vector<FSceneDataSet> m_allFSceneDataSets;
};