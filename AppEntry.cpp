//
// AppEntry.cpp
//

#include "AppEntry.h"

extern void ExitGame();

AppEntry::AppEntry() noexcept(false)
{
    m_deviceResources = std::make_unique<DeviceResources>(
		m_backBufferFormat, 
		m_depthStencilFormat,
		m_numFrameResources);

    m_deviceResources->RegisterDeviceNotify(this);
}

AppEntry::~AppEntry()
{
    if (m_deviceResources)
    {
        m_deviceResources->WaitForGpu();
		if (m_appGui)
		{
			m_appGui->DestroyGUI();
		}		
    }
}

// Initialize the Direct3D resources required to run.
void AppEntry::Initialize(HWND window, int width, int height)
{
	m_window = window;
	m_width = width;
	m_height = height;

    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources(); // Second Initialize.

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
	
	m_appGui->InitGUI();

    // TODO: Change the timer settings if you want something other than the default variable timestep mode.
    // e.g. for 60 FPS fixed timestep update logic, call:
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetDeltaSeconds(1.0f / 60);
	
	// Initialize.
	m_camera.SetPosition(0.0f, 8.0f, -25.0f);
	m_camera.SetFrustum(0.25f*XM_PI, m_width, m_height, 1.0f, 2000.0f);
}

#pragma region Frame Update
// Executes the basic game loop.
void AppEntry::Tick()
{
    m_timer.Tick([&]()
    {
		OnKeyboardInput(m_timer);		
        Update(m_timer);		
    });

    Render();
}

// Updates the world.
void AppEntry::Update(const StepTimer& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

	// DO ONCE. GUI Options Changed.
	if (m_appGui->GetAppData()->bOptionsChanged)
	{
		m_appGui->GetAppData()->bOptionsChanged = false;
		if (m_appGui->GetAppData()->bEnable4xMsaa)
		{
			m_deviceResources->SetOptions(m_deviceResources->GetDeviceOptions() | DeviceResources::c_Enable4xMsaa);
		}
		else
		{
			m_deviceResources->SetOptions(m_deviceResources->GetDeviceOptions() & ~DeviceResources::c_Enable4xMsaa);
		}
	}

	// Grid Update.
	if (m_appGui->GetAppData()->bGridDirdy)
	{
		m_appGui->GetAppData()->bGridDirdy = false;		
		if (m_appGui->GetAppData()->GridUnit >= 2u || m_appGui->GetAppData()->GridUnit <= 2000u)
		{
			m_deviceResources->ExecuteCommandLists([&]()
			{
				MeshData<ColorVertex> gridMesh = GeometryCreator::CreateLineGrid(
					m_appGui->GetAppData()->GridWidth,
					m_appGui->GetAppData()->GridWidth,
					m_appGui->GetAppData()->GridUnit,
					m_appGui->GetAppData()->GridUnit);
				m_deviceResources->WaitForGpu();
				m_allRitems[0]->CreateCommonGeometry<ColorVertex, uint32>(m_deviceResources.get(), m_allRitems[0]->Name, gridMesh.Vertices, gridMesh.Indices32);
			});
		}	
	}

	// FSceneDataImporter.
	if (m_appGui->CheckImporterLock())
	{
		if (m_appGui->GetImporterData()->GetDataDirtyFlag())
		{
			m_appGui->GetImporterData()->SetDataDirtyFlag(false);

			if (m_appGui->GetImporterData()->GetFSceneData(0))
			{
				if (!m_appGui->GetImporterData()->GetFSceneData(0)->StaticMeshesTable.empty() ||
					!m_appGui->GetImporterData()->GetFSceneData(0)->SkeletalMeshesTable.empty())
				{
					m_allFSceneDataSets.push_back(*m_appGui->GetImporterData()->GetFSceneData(0));
					m_deviceResources->ExecuteCommandLists([&]()
					{
						BuildFSceneRenderItems(m_appGui->GetImporterData()->GetFSceneData(0));
					});

					// Above already Call to Wait for Gpu.
					m_frameResource->ResizeBuffer<ObjectConstant>((UINT)m_allRitems.size());
					UINT structBufferCount = 0;
					for (auto& fSceneDataSet : m_allFSceneDataSets)
					{
						for (auto& staticMesh : fSceneDataSet.StaticMeshesTable)
						{
							structBufferCount += (UINT)staticMesh.BoundsIndices.size();
						}
						structBufferCount += (UINT)fSceneDataSet.SkeletalMeshesTable.size();
					}				
					m_frameResource->ResizeBuffer<StructureBuffer>(structBufferCount);
					// CBuffer Changed.
					for (auto& ri : m_allRitems)
						ri->bObjectDataChanged = true;
				}
			}		
		}
	}

	// Clear FScene.
	if (m_appGui->GetAppData()->bClearFScene)
	{
		m_appGui->GetAppData()->bClearFScene = false;
		
		m_deviceResources->WaitForGpu();
		RenderItem::ObjectCount = 1;
		m_allRitems.resize(1);
		m_renderItemLayer[RenderLayer::Opaque].clear();
		m_perFSceneCPUSBuffer.clear();
		
		m_frameResource->ResizeBuffer<ObjectConstant>((UINT)m_allRitems.size());
		m_frameResource->ResizeBuffer<StructureBuffer>((UINT)m_perFSceneCPUSBuffer.size());
	}

	// Update Camera & Frame Resource.
	{
		// Set Camera View Type.
#define CV_SetView(x) case x:m_camera.SetViewType(x);break;
		switch (m_appGui->GetAppData()->ECameraViewType)
		{
			CV_SetView(CV_FirstPersonView);
			CV_SetView(CV_FocusPointView);
			CV_SetView(CV_TopView);
			CV_SetView(CV_BottomView);
			CV_SetView(CV_LeftView);
			CV_SetView(CV_RightView);
			CV_SetView(CV_FrontView);
			CV_SetView(CV_BackView);
		default:
			break;
		}
#define CP_SetProj(x) case x:m_camera.SetProjType(x);break;
		switch (m_appGui->GetAppData()->ECameraProjType)
		{
			CP_SetProj(CP_PerspectiveProj);
			CP_SetProj(CP_OrthographicProj);
		default:
			break;
		}

		m_camera.UpdateViewMatrix();

		// PerPass Constant Buffer.
		PassConstant passConstant;
		passConstant.ViewProj = Matrix4(m_camera.GetViewProj());

		m_frameResource->CopyData<PassConstant>(0, passConstant);

		// PerObject Constant Buffer.
		m_allRitems.front()->World = m_appGui->GetAppData()->LocalToWorld;

		for (auto& ri : m_allRitems)
		{
			ri->bObjectDataChanged = true; // update every frame.
			if (ri->bObjectDataChanged)
			{
				ri->bObjectDataChanged = false;

				ObjectConstant objectConstant;
				objectConstant.World = ri->World;
				objectConstant.Overflow = m_appGui->GetAppData()->Overflow;
				objectConstant.PerFSceneSBufferOffset = ri->PerFSceneSBufferOffset;

				m_frameResource->CopyData<ObjectConstant>(ri->ObjectCBufferIndex, objectConstant);
			}			
		}

		// Structure Buffer.
		if (!m_allFSceneDataSets.empty())
		{
			if (m_appGui->GetAppData()->bVisualizationAttributeDirty)
			{
				m_appGui->GetAppData()->bVisualizationAttributeDirty = false;
				m_perFSceneCPUSBuffer.clear();
				for (auto& fSceneDataSet : m_allFSceneDataSets)
				{
					for (auto& staticMesh : fSceneDataSet.StaticMeshesTable)
					{
						for (int i = 0; i < staticMesh.BoundsIndices.size(); ++i)
						{
							// Fill Per FScene CPU Structure Buffer.
							float colorX = 0.0f;
							switch (m_appGui->GetAppData()->EVisualizationAttribute)
							{
							case VA_NumVertices:
								colorX = (float)staticMesh.NumVertices;
								break;
							case VA_NumTriangles:
								colorX = (float)staticMesh.NumTriangles;
								break;
							case VA_NumInstances:
								colorX = (float)staticMesh.NumInstances;
								break;
							case VA_NumLODs:
								colorX = (float)staticMesh.NumLODs;
								break;
							case VA_NumMaterials:
								colorX = (float)(staticMesh.UsedMaterialsIndices.size() + staticMesh.UsedMaterialIntancesIndices.size());
								break;
							case VA_NumTextures:
							{
								std::map<int32, void*> temp;
								for (auto& matID : staticMesh.UsedMaterialsIndices)
								{
									for (auto& texID : fSceneDataSet.MaterialsTable[matID].UsedTexturesIndices)
									{
										temp.emplace(fSceneDataSet.TexturesTable[texID].UniqueId, nullptr);
									}
								}
								for (auto& matID : staticMesh.UsedMaterialIntancesIndices)
								{
									for (auto& texID : fSceneDataSet.MaterialInstancesTable[matID].UsedTexturesIndices)
									{
										temp.emplace(fSceneDataSet.TexturesTable[texID].UniqueId, nullptr);
									}
								}
								colorX = (float)temp.size();
							}
							break;
							default:
								break;
							}
							StructureBuffer sBuffer;
							sBuffer.Color = Vector4(colorX, colorX, colorX, 1.0f);
							m_perFSceneCPUSBuffer.push_back(sBuffer);
						}						
					}
					for (auto& skeletalMesh : fSceneDataSet.SkeletalMeshesTable)
					{
						// Fill Per FScene CPU Structure Buffer.
						float colorX = 0.0f;
						switch (m_appGui->GetAppData()->EVisualizationAttribute)
						{
						case VA_NumVertices:
							colorX = (float)skeletalMesh.NumVertices;
							break;
						case VA_NumTriangles:
							colorX = (float)skeletalMesh.NumTriangles;
							break;
						case VA_NumLODs:
							colorX = (float)skeletalMesh.NumLODs;
							break;
						case VA_NumMaterials:
							colorX = (float)(skeletalMesh.UsedMaterialsIndices.size() + skeletalMesh.UsedMaterialIntancesIndices.size());
							break;
						case VA_NumTextures:
						{
							std::map<int32, void*> temp;
							for (auto& matID : skeletalMesh.UsedMaterialsIndices)
							{
								for (auto& texID : fSceneDataSet.MaterialsTable[matID].UsedTexturesIndices)
								{
									temp.emplace(fSceneDataSet.TexturesTable[texID].UniqueId, nullptr);
								}
							}
							for (auto& matID : skeletalMesh.UsedMaterialIntancesIndices)
							{
								for (auto& texID : fSceneDataSet.MaterialInstancesTable[matID].UsedTexturesIndices)
								{
									temp.emplace(fSceneDataSet.TexturesTable[texID].UniqueId, nullptr);
								}
							}
							colorX = (float)temp.size();
						}
						break;
						default:
							break;
						}
						StructureBuffer sBuffer;
						sBuffer.Color = Vector4(colorX, colorX, colorX, 1.0f);
						m_perFSceneCPUSBuffer.push_back(sBuffer);
					}
				}			
				for (int i = 0; i < m_perFSceneCPUSBuffer.size(); ++i)
				{
					m_frameResource->CopyData<StructureBuffer>(i, m_perFSceneCPUSBuffer[i]);
				}
			}
		}		
	}

    PIXEndEvent();
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void AppEntry::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();
    
    auto commandList = m_deviceResources->GetCommandList();

	// Clear the back buffers.
	PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");
	m_deviceResources->Clear((float*)&m_appGui->GetAppData()->ClearColor);
	PIXEndEvent(commandList);

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    // TODO: Add your rendering code here.
	{
		ID3D12DescriptorHeap* descriptorHeaps[] = { m_srvCbvDescHeap.Get() };
		commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);		

		commandList->SetGraphicsRootSignature(m_rootSignature.Get());

		// Set PerPass Data.
		commandList->SetGraphicsRootConstantBufferView(0, m_frameResource->GetBufferGPUVirtualAddress<PassConstant>());	

		if (m_appGui->GetAppData()->bShowGrid)
		{
			commandList->SetPipelineState(m_PSOs["Line"].Get());
			DrawRenderItem(m_renderItemLayer[RenderLayer::Line]);
		}	

		// Set Structure Buffer.
		if (!m_perFSceneCPUSBuffer.empty())
		{
			commandList->SetGraphicsRootShaderResourceView(2, m_frameResource->GetBufferGPUVirtualAddress<StructureBuffer>());
			switch (m_appGui->GetAppData()->EVisualizationColorMode)
			{
			case VCM_ColorWhite:
				commandList->SetPipelineState(m_PSOs["Box"].Get());
				break;
			case VCM_ColorRGB:
				commandList->SetPipelineState(m_PSOs["BoxRGB"].Get());
				break;
			default:
				break;
			}
			DrawRenderItem(m_renderItemLayer[RenderLayer::Opaque]);
		}				
		
	}

    PIXEndEvent(commandList);

    // Show the new frame.
    PIXBeginEvent(m_deviceResources->GetCommandQueue(), PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    PIXEndEvent(m_deviceResources->GetCommandQueue());
}

void AppEntry::DrawRenderItem(std::vector<RenderItem*>& renderItemLayer)
{
	auto commandList = m_deviceResources->GetCommandList();

	for (auto& ri : renderItemLayer)
	{
		ri->Draw(commandList, [&]()
		{
			UINT objectCBufferByteSize = AppUtil::CalcConstantBufferByteSize(sizeof(ObjectConstant));
			D3D12_GPU_VIRTUAL_ADDRESS objectCBufferAddress = m_frameResource->GetBufferGPUVirtualAddress<ObjectConstant>()
				+ ri->ObjectCBufferIndex * objectCBufferByteSize;
			commandList->SetGraphicsRootConstantBufferView(1, objectCBufferAddress);
		});
	}
}

#pragma endregion

#pragma region Message Handlers
// Message handlers
void AppEntry::OnActivated()
{
    // TODO: Game is becoming active window.
}

void AppEntry::OnDeactivated()
{
    // TODO: Game is becoming background window.
}

void AppEntry::OnSuspending()
{
    // TODO: Game is being power-suspended (or minimized).
}

void AppEntry::OnResuming()
{
	m_timer.Reset();

    // TODO: Game is being power-resumed (or returning from minimize).
}

void AppEntry::OnWindowMoved()
{
    auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void AppEntry::OnWindowSizeChanged(int width, int height)
{
	m_deviceResources->WaitForGpu();
	m_appGui->GUIInvalidateDeviceObjects();

	m_width  = width;
	m_height = height;

    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();

	m_appGui->GUICreateDeviceObjects();

    // TODO: Game window is being resized.

	// OnWindowSizeChanged.
	m_camera.SetFrustum(0.25f*XM_PI, m_width, m_height, 1.0f, 2000.0f);
}

void AppEntry::OnMouseDown(WPARAM btnState, int x, int y)
{
	if (CheckInBlockAreas(x, y))
	{
		m_bMouseDownInBlockArea = true;
		return;
	}
	m_bMouseDownInBlockArea = false;

	if (btnState == MK_LBUTTON || btnState == MK_RBUTTON || btnState == MK_MBUTTON)
	{
		m_lastMousePos.x = x;
		m_lastMousePos.y = y;
		
		SetCapture(m_window);		
	}
}

void AppEntry::OnMouseUp(WPARAM btnState, int x, int y)
{
	if (CheckInBlockAreas(x, y))
		return;
	ReleaseCapture();
}

void AppEntry::OnMouseMove(WPARAM btnState, int x, int y)
{
	if (m_bMouseDownInBlockArea)
		return;

	if (btnState == MK_LBUTTON || btnState == MK_MBUTTON)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f*static_cast<float>(x - m_lastMousePos.x));
		float dy = XMConvertToRadians(0.25f*static_cast<float>(y - m_lastMousePos.y));

		m_camera.ProcessMoving(dx, dy);
	}
	else if (btnState == MK_RBUTTON)
	{
		// Make each pixel correspond to 0.005 unit in the scene.
		float dx = 0.005f*static_cast<float>(x - m_lastMousePos.x);
		float dy = 0.005f*static_cast<float>(y - m_lastMousePos.y);
		m_camera.FocusRadius(dx - dy);
	}

	m_lastMousePos.x = x;
	m_lastMousePos.y = y;
}

void AppEntry::OnMouseWheel(int d, WPARAM btnState, int x, int y)
{
	m_camera.FocusRadius(0.02f*(float)d);
}

void AppEntry::OnKeyDown(WPARAM keyState)
{
	
}

void AppEntry::OnKeyUp(WPARAM keyState)
{

}

void AppEntry::OnKeyboardInput(const StepTimer& timer)
{
	const float dt = (float)timer.GetDeltaSeconds();

	if (GetAsyncKeyState('W') & 0x8000)
		m_camera.Walk(10.0f*dt);

	if (GetAsyncKeyState('S') & 0x8000)
		m_camera.Walk(-10.0f*dt);

	if (GetAsyncKeyState('A') & 0x8000)
		m_camera.Strafe(-10.0f*dt);

	if (GetAsyncKeyState('D') & 0x8000)
		m_camera.Strafe(10.0f*dt);
}

// GUI Messages
bool AppEntry::CheckInBlockAreas(int x, int y)
{
	for (auto& blockArea : m_appGui->GetAppData()->BlockAreas)
	{
		if (blockArea == nullptr)
			continue;

		if (x >= blockArea->StartPos.x &&
			y >= blockArea->StartPos.y &&
			x <= (blockArea->StartPos.x + blockArea->BlockSize.x) &&
			y <= (blockArea->StartPos.y + blockArea->BlockSize.y))
			return true;
	}
	return false;
}

LRESULT AppEntry::AppMessageProcessor(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return m_appGui->GUIMessageProcessor(hWnd, msg, wParam, lParam);
}

// Properties
void AppEntry::GetDefaultSize(int& width, int& height) const
{
    // TODO: Change to desired default window size (note minimum size is 320x200).
    width  = 1280;
    height = 720;
}

void AppEntry::GetCurrentSize(int& width, int& height) const
{
	width  = m_width;
	height = m_height;
}

float AppEntry::AspectRatio() const
{
	return static_cast<float>(m_width) / m_height;
}

#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void AppEntry::CreateDeviceDependentResources()
{
	auto device = m_deviceResources->GetD3DDevice();
	auto commandList = m_deviceResources->GetCommandList();

	Window window = { m_window, XMINT2(m_width, m_height) };
	m_appGui = std::make_unique<AppGUI>(window, device, commandList, m_deviceResources->GetBackBufferCount());	

    // TODO: Initialize device dependent objects here (independent of window size).
	m_deviceResources->ExecuteCommandLists([&]()
	{
		// Create Srv Desc for GUI And ...
		BuildDescriptorHeaps();
		BuildConstantBuffers();
		BuildRootSignature();
		BuildShadersAndInputLayout();
		BuildLineGridGeometry();
		BuildPSO();	
	});
	m_frameResource = std::make_unique<FrameResource>(m_deviceResources->GetD3DDevice(), 1, (UINT)m_allRitems.size());
}

// Allocate all memory resources that change on a window SizeChanged event.
void AppEntry::CreateWindowSizeDependentResources()
{
    // TODO: Initialize windows-size dependent objects here.
}

void AppEntry::BuildDescriptorHeaps()
{
	m_deviceResources->CreateCommonDescriptorHeap(m_appGui->GetDescriptorCount(), &m_srvCbvDescHeap);
	m_appGui->SetSrvDescHeap(m_srvCbvDescHeap.Get());
}

void AppEntry::BuildConstantBuffers()
{
	
}

void AppEntry::BuildRootSignature()
{
	// Root parameter can be a table, root descriptor or root constants.
	const unsigned int NUM_ROOTPARAMETER = 3;
	CD3DX12_ROOT_PARAMETER slotRootParameter[NUM_ROOTPARAMETER];

	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstantBufferView(1);
	slotRootParameter[2].InitAsShaderResourceView(0);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(NUM_ROOTPARAMETER, slotRootParameter, 0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	m_deviceResources->CreateRootSignature(&rootSigDesc, &m_rootSignature);

	// NOTE: Shader programs typically require resources as input (constant buffers,
	// textures, samplers).  The root signature defines the resources the shader
	// programs expect.  If we think of the shader programs as a function, and
	// the input resources as function parameters, then the root signature can be
	// thought of as defining the function signature.  
}

void AppEntry::BuildShadersAndInputLayout()
{
	m_shaderByteCode["BoxVS"] = AppUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_1");
	m_shaderByteCode["BoxPS"] = AppUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_1");

	m_shaderByteCode["BoxRGBVS"] = AppUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "RGBVS", "vs_5_1");

	m_shaderByteCode["LineVS"] = AppUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "LineVS", "vs_5_1");

	m_inputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void AppEntry::BuildLineGridGeometry()
{
	auto gridRItem = std::make_unique<RenderItem>();
	gridRItem->Name = "grid";
	gridRItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	MeshData<ColorVertex> gridMesh = GeometryCreator::CreateLineGrid(
		m_appGui->GetAppData()->GridWidth,
		m_appGui->GetAppData()->GridWidth,
		m_appGui->GetAppData()->GridUnit,
		m_appGui->GetAppData()->GridUnit);
	gridRItem->CreateCommonGeometry<ColorVertex, uint16>(m_deviceResources.get(), gridRItem->Name, gridMesh.Vertices, gridMesh.GetIndices16());

	m_renderItemLayer[RenderLayer::Line].push_back(gridRItem.get());
	m_allRitems.push_back(std::move(gridRItem));
}

void AppEntry::BuildFSceneRenderItems(const FSceneDataSet* currentFSceneDataSet)
{
	MeshData<ColorVertex> boxMesh = GeometryCreator::CreateDefaultBox();
	MeshData<ColorVertex> fSceneMesh;
	int perFSceneBoxCount = 0; // bounds to box.

	for (auto& staticMesh : currentFSceneDataSet->StaticMeshesTable)
	{
		for (auto& boundsIndex : staticMesh.BoundsIndices)
		{
			BoxSphereBounds bounds = currentFSceneDataSet->BoundsTable[boundsIndex];
			XMFLOAT3 corners[8]; int i = 0;
			bounds.BoxBounds.GetCorners(corners);
			for (auto& vertex : boxMesh.Vertices)
			{
				vertex.Pos = Vector3(corners[i++]);
				vertex.Pos.RightHandToLeft();
				fSceneMesh.Vertices.push_back(vertex);
			}
			for (auto& index : boxMesh.Indices32)
			{
				fSceneMesh.Indices32.push_back(index + perFSceneBoxCount * 8);
			}
			perFSceneBoxCount++;			
		}		
	}

	for (auto& skeletalMesh : currentFSceneDataSet->SkeletalMeshesTable)
	{
		BoxSphereBounds bounds = currentFSceneDataSet->BoundsTable[skeletalMesh.BoundsIndex];
		XMFLOAT3 corners[8]; int i = 0;
		bounds.BoxBounds.GetCorners(corners);
		for (auto& vertex : boxMesh.Vertices)
		{
			vertex.Pos = Vector3(corners[i++]);
			vertex.Pos.RightHandToLeft();
			fSceneMesh.Vertices.push_back(vertex);
		}
		for (auto& index : boxMesh.Indices32)
		{
			fSceneMesh.Indices32.push_back(index + perFSceneBoxCount * 8);
		}
		perFSceneBoxCount++;		
	}

	auto boxRItem = std::make_unique<RenderItem>();
	boxRItem->Name = "box" + std::to_string(boxRItem->ObjectCBufferIndex);
	boxRItem->World = Matrix4(AffineTransform::MakeScale(0.001f));
	boxRItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRItem->PerFSceneSBufferOffset = (int)m_perFSceneCPUSBuffer.size();
	boxRItem->CreateCommonGeometry<ColorVertex, uint16>(m_deviceResources.get(), boxRItem->Name, fSceneMesh.Vertices, fSceneMesh.GetIndices16());

	m_renderItemLayer[RenderLayer::Opaque].push_back(boxRItem.get());
	m_allRitems.push_back(std::move(boxRItem));
}

void AppEntry::BuildPSO()
{
	bool enable4xMsaa = m_deviceResources->GetDeviceOptions() & DeviceResources::c_Enable4xMsaa;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc0;
	ZeroMemory(&psoDesc0, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	// Common.
	psoDesc0.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
	psoDesc0.pRootSignature = m_rootSignature.Get();

	// Shader.
	psoDesc0.VS =
	{
		reinterpret_cast<BYTE*>(m_shaderByteCode["BoxVS"]->GetBufferPointer()),
		m_shaderByteCode["BoxVS"]->GetBufferSize()
	};
	psoDesc0.PS =
	{
		reinterpret_cast<BYTE*>(m_shaderByteCode["BoxPS"]->GetBufferPointer()),
		m_shaderByteCode["BoxPS"]->GetBufferSize()
	};

	// Default.
	psoDesc0.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc0.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc0.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

	psoDesc0.DepthStencilState.DepthEnable = false;
	psoDesc0.BlendState.RenderTarget[0].BlendEnable = true;
	psoDesc0.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;

	psoDesc0.SampleMask = UINT_MAX;
	psoDesc0.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc0.NumRenderTargets = 1;
	psoDesc0.RTVFormats[0] = m_backBufferFormat;
	psoDesc0.SampleDesc.Count =  enable4xMsaa ? 4 : 1;
	psoDesc0.SampleDesc.Quality = enable4xMsaa ? (m_deviceResources->GetNum4MSQualityLevels() - 1) : 0;
	psoDesc0.DSVFormat = m_depthStencilFormat;

	m_deviceResources->CreateGraphicsPipelineState(&psoDesc0, &m_PSOs["Box"]);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc1 = psoDesc0;
	psoDesc1.VS =
	{
		reinterpret_cast<BYTE*>(m_shaderByteCode["LineVS"]->GetBufferPointer()),
		m_shaderByteCode["LineVS"]->GetBufferSize()
	};
	psoDesc1.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc1.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc1.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	m_deviceResources->CreateGraphicsPipelineState(&psoDesc1, &m_PSOs["Line"]);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc2 = psoDesc0;
	psoDesc2.VS = 
	{
		reinterpret_cast<BYTE*>(m_shaderByteCode["BoxRGBVS"]->GetBufferPointer()),
		m_shaderByteCode["BoxRGBVS"]->GetBufferSize()
	};
	m_deviceResources->CreateGraphicsPipelineState(&psoDesc2, &m_PSOs["BoxRGB"]);
}

#pragma endregion

#pragma region Interface IDeviceNotify
// IDeviceNotify
void AppEntry::OnDeviceLost()
{
    // TODO: Add Direct3D resource cleanup here.
	m_srvCbvDescHeap.Reset();
	m_rootSignature.Reset();
	for (auto& e : m_PSOs)
		e.second.Reset();

	for (auto& e : m_shaderByteCode)
		e.second.Reset();
}

void AppEntry::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}

// Draw GUI.
void AppEntry::RenderGUI()
{
	m_appGui->RenderGUI();
}

void AppEntry::OnOptionsChanged()
{
	// 4xMsaa.
	BuildPSO();
}

#pragma endregion
