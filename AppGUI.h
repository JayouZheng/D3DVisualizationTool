//
// AppGUI.h
//

#pragma once

#include "AppUtil.h"
#include "AppData.h"
#include "UnrealEngine/FSceneDataImporter.h"
#include "Common/ThreadManager.h"
#include "Common/TimerManager.h"

using namespace DX;
using namespace UnrealEngine;

class AppGUI
{
public:

	AppGUI(Window InWindow, ID3D12Device* InDevice, ID3D12GraphicsCommandList* InCmdList, UINT InNumFrameResources);
	~AppGUI();

	bool InitGUI();
	void RenderGUI();
	void DestroyGUI();

	//////////////////////////////////
	// Call when resizing.
	void GUIInvalidateDeviceObjects();
	bool GUICreateDeviceObjects();
	//////////////////////////////////

	void SetSrvDescHeap(ID3D12DescriptorHeap* InSrvDescHeap, INT InOffset = 0);

	LRESULT GUIMessageProcessor(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	// Properties
	UINT	 GetDescriptorCount() const { return 1; }
	AppData* GetAppData()		  const { return m_appData.get(); }
	FSceneDataImporter* GetImporterData() const { return m_importer.get(); }

	bool CheckImporterLock() const { return m_importerLock; }

private:

	void NewFrame();
	void DrawGUI();

	// Note: Put struct member first, structure was padded due to alignment specifier...

	std::unique_ptr<AppData> m_appData = nullptr;
	std::unique_ptr<FSceneDataImporter> m_importer = nullptr;

	Window m_window;
	ID3D12Device*			   m_d3dDevice = nullptr;
	ID3D12GraphicsCommandList* m_d3dCommandList = nullptr;
	ID3D12DescriptorHeap*	   m_d3dSrvDescHeap = nullptr;
	INT  m_offsetInDescs = 0; // From HeapStart.
	UINT m_descIncrementSize; // Runtime Data... m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	
	UINT m_numFrameResources; // Must be the same with app DR.

	// Client.
	bool m_bShowDemo = true;

	// Others.
	std::atomic<bool> m_importerLock = false;
	TimerManager::PerformanceCounter m_performanceCounter;
};