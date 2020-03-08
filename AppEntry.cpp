//
// AppEntry.cpp
//

#include "AppEntry.h"
#include "Common/StringManager.h"

using namespace DX::StringManager;

extern void ExitGame();

AppEntry::AppEntry(std::wstring appPath) noexcept(false)
{
    m_deviceResources = std::make_unique<DeviceResources>(
		m_backBufferFormat, 
		m_depthStencilFormat,
		m_numFrameResources);

    m_deviceResources->RegisterDeviceNotify(this);
	m_appPath = appPath;
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
void AppEntry::Initialize(HWND window, int width, int height, std::wstring cmdLine)
{
	m_window = window;
	m_width = width;
	m_height = height;

    m_deviceResources->SetWindow(window, width, height, cmdLine);

    m_deviceResources->CreateDeviceResources();
	// Create AppGui Instance & ...
    CreateDeviceDependentResources();
	
    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

	m_appGui->GetAppData()->AppPath = m_appPath;
	m_appGui->InitGUI(cmdLine);

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
					// After resize StructureBuffer, Update is Needed.
					m_appGui->GetAppData()->bVisualizationAttributeDirty = true;
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
		m_renderItemLayer[RenderLayer::FScene].clear();
		m_perFSceneCPUSBuffer.clear();
		
		m_frameResource->ResizeBuffer<ObjectConstant>((UINT)m_allRitems.size());
		m_frameResource->ResizeBuffer<StructureBuffer>((UINT)m_perFSceneCPUSBuffer.size());
	}

	// Find Max Pixel On the CPU side.
	if (m_appGui->GetAppData()->bEnableCalcMax)
	{		
		Math::Vector4* mappedData = nullptr;
		ThrowIfFailed(m_readBackBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)));
		Math::Vector4 maxPixel = Math::Vector4(Math::EZeroTag::kZero);
		for (int i = 0; i < m_height; ++i)
		{
			maxPixel = Math::Max(maxPixel, mappedData[i]);
		}
		m_appGui->GetAppData()->MaxPixel = maxPixel;
		m_readBackBuffer->Unmap(0, nullptr);
	}

	// Update Camera & Frame Resource.
	{
		// Set Camera View Type.
#define CV_SetView(x) case x:m_camera.SetViewType(x);break;
		switch (m_appGui->GetAppData()->_ECameraViewType)
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
		switch (m_appGui->GetAppData()->_ECameraProjType)
		{
			CP_SetProj(CP_PerspectiveProj);
			CP_SetProj(CP_OrthographicProj);
		default:
			break;
		}

		m_camera.UpdateViewMatrix();

		// Update Camera FarZ.
		if (m_appGui->GetAppData()->bCameraFarZDirty)
		{
			m_appGui->GetAppData()->bCameraFarZDirty = false;
			m_camera.SetFarZ(m_appGui->GetAppData()->CameraFarZ);
		}

		// PerPass Constant Buffer.
		PassConstant passConstant;
		passConstant.ViewProj = Matrix4(m_camera.GetViewProj());
		passConstant.Overflow = m_appGui->GetAppData()->Overflow;
		m_frameResource->CopyData<PassConstant>(0, passConstant);

		// PerObject Constant Buffer (Line).
		for (auto& ri : m_renderItemLayer[RenderLayer::Line])
		{
			ri->bObjectDataChanged = true; // update every frame.
			if (ri->bObjectDataChanged)
			{
				ri->bObjectDataChanged = false;
				ObjectConstant objectConstant;
				objectConstant.World = m_appGui->GetAppData()->LocalToWorld;
				m_frameResource->CopyData<ObjectConstant>(ri->ObjectCBufferIndex, objectConstant);
			}
		}

		// PerObject Constant Buffer (FScene).
		for (auto& ri : m_renderItemLayer[RenderLayer::FScene])
		{
			ri->bObjectDataChanged = true; // update every frame.
			if (ri->bObjectDataChanged)
			{
				ri->bObjectDataChanged = false;
				ObjectConstant objectConstant;
				objectConstant.World = Matrix4(AffineTransform::MakeScale(m_appGui->GetAppData()->FSceneScale));
				objectConstant.PerFSceneSBufferOffset = ri->PerFSceneSBufferOffset;
				m_frameResource->CopyData<ObjectConstant>(ri->ObjectCBufferIndex, objectConstant);
			}			
		}

		// Structure Buffer (Visualization Attribute Logic).
		if (!m_allFSceneDataSets.empty())
		{
			if (m_appGui->GetAppData()->bVisualizationAttributeDirty)
			{
				m_appGui->GetAppData()->bVisualizationAttributeDirty = false;
				m_perFSceneCPUSBuffer.clear();
				for (auto& fSceneDataSet : m_allFSceneDataSets)
				{
#pragma region LocalUsefulDefine
#define SetColorX(y, x) case VA_##x: colorX = (float)y.x; break;
#define SetColorXCaseMatProp(x, y, z) \
{ \
	case x: \
	{ \
		for (auto& matID : y.UsedMaterialsIndices) \
			colorX += (float)fSceneDataSet.MaterialsTable[matID].z; \
		for (auto& matInsID : y.UsedMaterialIntancesIndices) \
			colorX += (float)fSceneDataSet.MaterialsTable[fSceneDataSet.MaterialInstancesTable[matInsID].ParentIndex].z; \
	} \
	break; \
}
#define SetColorXCaseMatPropString(x, y, z) \
{ \
	case x: \
	{ \
		for (auto& matID : y.UsedMaterialsIndices) \
		{ \
			colorX += StringUtil::WStringToNumeric<float>( \
				fSceneDataSet.MaterialsTable[matID].z); \
		} \
		for (auto& matInsID : y.UsedMaterialIntancesIndices) \
		{ \
			colorX += StringUtil::WStringToNumeric<float>( \
				fSceneDataSet.MaterialsTable[fSceneDataSet.MaterialInstancesTable[matInsID].ParentIndex].z); \
		} \
	} \
	break; \
}
#define SetColorXCaseMatPropStringGetBetween(x, y, z, b1, b2) \
{ \
	case x: \
	{ \
		for (auto& matID : y.UsedMaterialsIndices) \
		{ \
			colorX += StringUtil::WStringToNumeric<float>( \
				StringUtil::WGetBetween(fSceneDataSet.MaterialsTable[matID].z, b1, b2).front()); \
		} \
		for (auto& matInsID : y.UsedMaterialIntancesIndices) \
		{ \
			colorX += StringUtil::WStringToNumeric<float>( \
				StringUtil::WGetBetween(fSceneDataSet.MaterialsTable[fSceneDataSet.MaterialInstancesTable[matInsID].ParentIndex].z, b1, b2).front()); \
		} \
	} \
	break; \
}
#pragma endregion
					for (auto& staticMesh : fSceneDataSet.StaticMeshesTable)
					{
						for (int i = 0; i < staticMesh.BoundsIndices.size(); ++i)
						{
							// Fill Per FScene CPU Structure Buffer.
							float colorX = 0.0f;

							// Calculate the unique texture data for StaticMesh.
							std::map<int32, int32> uniqueTexIDs;
							{
								for (auto& matID : staticMesh.UsedMaterialsIndices)
								{
									for (auto& texID : fSceneDataSet.MaterialsTable[matID].UsedTexturesIndices)
									{
										uniqueTexIDs.emplace(fSceneDataSet.TexturesTable[texID].UniqueId, texID);
									}
								}
								for (auto& matInsID : staticMesh.UsedMaterialIntancesIndices)
								{
									for (auto& texID : fSceneDataSet.MaterialInstancesTable[matInsID].UsedTexturesIndices)
									{
										uniqueTexIDs.emplace(fSceneDataSet.TexturesTable[texID].UniqueId, texID);
									}
								}
							}
							
							// Calculate the specific property for StaticMesh.
							switch (m_appGui->GetAppData()->_EVisualizationAttribute)
							{
								SetColorX(staticMesh, NumVertices);
								SetColorX(staticMesh, NumTriangles);
								SetColorX(staticMesh, NumInstances);
								SetColorX(staticMesh, NumLODs);
							case VA_NumMaterials:
								colorX = (float)(staticMesh.UsedMaterialsIndices.size() + staticMesh.UsedMaterialIntancesIndices.size());
								break;
							case VA_NumTextures:
								colorX = (float)uniqueTexIDs.size();
								break;
							case VA_CurrentKB:
								for (auto& texID : uniqueTexIDs)
									colorX += fSceneDataSet.TexturesTable[texID.second].CurrentKB;
								break;
								SetColorXCaseMatProp(VA_Stats_Base_Pass_Shader_Instructions, staticMesh, BPSCount);
								SetColorXCaseMatProp(VA_Stats_Base_Pass_Shader_With_Surface_Lightmap, staticMesh, BPSSurfaceLightmap);
								SetColorXCaseMatProp(VA_Stats_Base_Pass_Shader_With_Volumetric_Lightmap, staticMesh, BPSVolumetricLightmap);
								SetColorXCaseMatProp(VA_Stats_Base_Pass_Vertex_Shader, staticMesh, BPSVertex);

								SetColorXCaseMatPropStringGetBetween(VA_Stats_Texture_Samplers, staticMesh, TexSamplers, L"_", L"/16");
								SetColorXCaseMatPropStringGetBetween(VA_Stats_User_Interpolators_Scalars, staticMesh, UserInterpolators, L"", L"/");
								SetColorXCaseMatPropStringGetBetween(VA_Stats_User_Interpolators_Vectors, staticMesh, UserInterpolators, L"(", L"/4 Vectors");
								SetColorXCaseMatPropStringGetBetween(VA_Stats_User_Interpolators_TexCoords, staticMesh, UserInterpolators, L"TexCoords: ", L",");
								SetColorXCaseMatPropStringGetBetween(VA_Stats_User_Interpolators_Custom, staticMesh, UserInterpolators, L"Custom: ", L")");
								SetColorXCaseMatPropStringGetBetween(VA_Stats_Texture_Lookups_VS, staticMesh, TexLookups, L"VS(", L")");
								SetColorXCaseMatPropStringGetBetween(VA_Stats_Texture_Lookups_PS, staticMesh, TexLookups, L"PS(", L")");
								
								SetColorXCaseMatPropString(VA_Stats_Virtual_Texture_Lookups, staticMesh, VTLookups);

								SetColorXCaseMatProp(VA_Material_Two_Sided, staticMesh, TwoSided);
								SetColorXCaseMatProp(VA_Material_Cast_Ray_Traced_Shadows, staticMesh, bCastRayTracedShadows);
								SetColorXCaseMatProp(VA_Translucency_Screen_Space_Reflections, staticMesh, bScreenSpaceReflections);
								SetColorXCaseMatProp(VA_Translucency_Contact_Shadows , staticMesh, bContactShadows);
								SetColorXCaseMatProp(VA_Translucency_Directional_Lighting_Intensity, staticMesh, TranslucencyDirectionalLightingIntensity);
								SetColorXCaseMatProp(VA_Translucency_Apply_Fogging, staticMesh, bUseTranslucencyVertexFog);
								SetColorXCaseMatProp(VA_Translucency_Compute_Fog_Per_Pixel, staticMesh, bComputeFogPerPixel);
								SetColorXCaseMatProp(VA_Translucency_Output_Velocity, staticMesh, bOutputTranslucentVelocity);
								SetColorXCaseMatProp(VA_Translucency_Render_After_DOF, staticMesh, bEnableSeparateTranslucency);
								SetColorXCaseMatProp(VA_Translucency_Responsive_AA, staticMesh, bEnableResponsiveAA);
								SetColorXCaseMatProp(VA_Translucency_Mobile_Separate_Translucency, staticMesh, bEnableMobileSeparateTranslucency);
								SetColorXCaseMatProp(VA_Translucency_Disable_Depth_Test, staticMesh, bDisableDepthTest);
								SetColorXCaseMatProp(VA_Translucency_Write_Only_Alpha, staticMesh, bWriteOnlyAlpha);
								SetColorXCaseMatProp(VA_Translucency_Allow_Custom_Depth_Writes, staticMesh, AllowTranslucentCustomDepthWrites);
								SetColorXCaseMatProp(VA_Mobile_Use_Full_Precision, staticMesh, bUseFullPrecision);
								SetColorXCaseMatProp(VA_Mobile_Use_Lightmap_Directionality, staticMesh, bUseLightmapDirectionality);
								SetColorXCaseMatProp(VA_Forward_Shading_High_Quality_Reflections, staticMesh, bUseHQForwardReflections);
								SetColorXCaseMatProp(VA_Forward_Shading_Planar_Reflections, staticMesh, bUsePlanarForwardReflections);
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

						// Calculate the unique texture data for SkeletalMesh.
						std::map<int32, int32> uniqueTexIDs;
						{
							for (auto& matID : skeletalMesh.UsedMaterialsIndices)
							{
								for (auto& texID : fSceneDataSet.MaterialsTable[matID].UsedTexturesIndices)
								{
									uniqueTexIDs.emplace(fSceneDataSet.TexturesTable[texID].UniqueId, texID);
								}
							}
							for (auto& matInsID : skeletalMesh.UsedMaterialIntancesIndices)
							{
								for (auto& texID : fSceneDataSet.MaterialInstancesTable[matInsID].UsedTexturesIndices)
								{
									uniqueTexIDs.emplace(fSceneDataSet.TexturesTable[texID].UniqueId, texID);
								}
							}
						}

						// Calculate the specific property for SkeletalMesh.
						switch (m_appGui->GetAppData()->_EVisualizationAttribute)
						{
							SetColorX(skeletalMesh, NumVertices);
							SetColorX(skeletalMesh, NumTriangles);
							SetColorX(skeletalMesh, NumLODs);
						case VA_NumMaterials:
							colorX = (float)(skeletalMesh.UsedMaterialsIndices.size() + skeletalMesh.UsedMaterialIntancesIndices.size());
							break;
						case VA_NumTextures:
							colorX = (float)uniqueTexIDs.size();
							break;
						case VA_CurrentKB:
							for (auto& texID : uniqueTexIDs)
								colorX += fSceneDataSet.TexturesTable[texID.second].CurrentKB;
							break;
							SetColorXCaseMatProp(VA_Stats_Base_Pass_Shader_Instructions, skeletalMesh, BPSCount);
							SetColorXCaseMatProp(VA_Stats_Base_Pass_Shader_With_Surface_Lightmap, skeletalMesh, BPSSurfaceLightmap);
							SetColorXCaseMatProp(VA_Stats_Base_Pass_Shader_With_Volumetric_Lightmap, skeletalMesh, BPSVolumetricLightmap);
							SetColorXCaseMatProp(VA_Stats_Base_Pass_Vertex_Shader, skeletalMesh, BPSVertex);

							SetColorXCaseMatPropStringGetBetween(VA_Stats_Texture_Samplers, skeletalMesh, TexSamplers, L"_", L"/16");
							SetColorXCaseMatPropStringGetBetween(VA_Stats_User_Interpolators_Scalars, skeletalMesh, UserInterpolators, L"", L"/");
							SetColorXCaseMatPropStringGetBetween(VA_Stats_User_Interpolators_Vectors, skeletalMesh, UserInterpolators, L"(", L"/4 Vectors");
							SetColorXCaseMatPropStringGetBetween(VA_Stats_User_Interpolators_TexCoords, skeletalMesh, UserInterpolators, L"TexCoords: ", L",");
							SetColorXCaseMatPropStringGetBetween(VA_Stats_User_Interpolators_Custom, skeletalMesh, UserInterpolators, L"Custom: ", L")");
							SetColorXCaseMatPropStringGetBetween(VA_Stats_Texture_Lookups_VS, skeletalMesh, TexLookups, L"VS(", L")");
							SetColorXCaseMatPropStringGetBetween(VA_Stats_Texture_Lookups_PS, skeletalMesh, TexLookups, L"PS(", L")");

							SetColorXCaseMatPropString(VA_Stats_Virtual_Texture_Lookups, skeletalMesh, VTLookups);

							SetColorXCaseMatProp(VA_Material_Two_Sided, skeletalMesh, TwoSided);
							SetColorXCaseMatProp(VA_Material_Cast_Ray_Traced_Shadows, skeletalMesh, bCastRayTracedShadows);
							SetColorXCaseMatProp(VA_Translucency_Screen_Space_Reflections, skeletalMesh, bScreenSpaceReflections);
							SetColorXCaseMatProp(VA_Translucency_Contact_Shadows, skeletalMesh, bContactShadows);
							SetColorXCaseMatProp(VA_Translucency_Directional_Lighting_Intensity, skeletalMesh, TranslucencyDirectionalLightingIntensity);
							SetColorXCaseMatProp(VA_Translucency_Apply_Fogging, skeletalMesh, bUseTranslucencyVertexFog);
							SetColorXCaseMatProp(VA_Translucency_Compute_Fog_Per_Pixel, skeletalMesh, bComputeFogPerPixel);
							SetColorXCaseMatProp(VA_Translucency_Output_Velocity, skeletalMesh, bOutputTranslucentVelocity);
							SetColorXCaseMatProp(VA_Translucency_Render_After_DOF, skeletalMesh, bEnableSeparateTranslucency);
							SetColorXCaseMatProp(VA_Translucency_Responsive_AA, skeletalMesh, bEnableResponsiveAA);
							SetColorXCaseMatProp(VA_Translucency_Mobile_Separate_Translucency, skeletalMesh, bEnableMobileSeparateTranslucency);
							SetColorXCaseMatProp(VA_Translucency_Disable_Depth_Test, skeletalMesh, bDisableDepthTest);
							SetColorXCaseMatProp(VA_Translucency_Write_Only_Alpha, skeletalMesh, bWriteOnlyAlpha);
							SetColorXCaseMatProp(VA_Translucency_Allow_Custom_Depth_Writes, skeletalMesh, AllowTranslucentCustomDepthWrites);
							SetColorXCaseMatProp(VA_Mobile_Use_Full_Precision, skeletalMesh, bUseFullPrecision);
							SetColorXCaseMatProp(VA_Mobile_Use_Lightmap_Directionality, skeletalMesh, bUseLightmapDirectionality);
							SetColorXCaseMatProp(VA_Forward_Shading_High_Quality_Reflections, skeletalMesh, bUseHQForwardReflections);
							SetColorXCaseMatProp(VA_Forward_Shading_Planar_Reflections, skeletalMesh, bUsePlanarForwardReflections);
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

		commandList->SetGraphicsRootSignature(m_ROOTSIGs["Main"].Get());

		// Set PerPass Data.
		commandList->SetGraphicsRootConstantBufferView(0, m_frameResource->GetBufferGPUVirtualAddress<PassConstant>());	

		if (m_appGui->GetAppData()->bShowGrid)
		{
			commandList->SetPipelineState(m_PSOs["Line"].Get());
			DrawRenderItem(m_renderItemLayer[RenderLayer::Line]);
		}	

		// Set Structure Buffer & Main drawing logic.
		if (!m_perFSceneCPUSBuffer.empty())
		{
			// Drawing on the Offscreen RT.
			commandList->SetGraphicsRootShaderResourceView(2, m_frameResource->GetBufferGPUVirtualAddress<StructureBuffer>());
			commandList->SetPipelineState(m_PSOs["BoxBlendAdd"].Get());
			commandList->OMSetRenderTargets(1, &m_deviceResources->GetOffscreenRenderTargetView(0), FALSE, nullptr);
			commandList->ClearRenderTargetView(m_deviceResources->GetOffscreenRenderTargetView(0), 
				DefaultClearValue::ColorRGBA, 0, nullptr);
			DrawRenderItem(m_renderItemLayer[RenderLayer::FScene]);

			// Bind Offscreen RT to Shader.
			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
				m_deviceResources->GetOffscreenRenderTarget(0),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_GENERIC_READ));			
			CD3DX12_GPU_DESCRIPTOR_HANDLE srvDescGPUHandle(
				m_srvCbvDescHeap->GetGPUDescriptorHandleForHeapStart(), 1,
				m_deviceResources->GetCbvSrvUavDescriptorSize());
			commandList->SetGraphicsRootDescriptorTable(3, srvDescGPUHandle);

			// Find Max Pixel On the GPU side.
			if (m_appGui->GetAppData()->bEnableCalcMax)
			{
				commandList->SetComputeRootSignature(m_ROOTSIGs["Main"].Get());
				commandList->SetPipelineState(m_PSOs["ComputeMax"].Get());
				commandList->SetComputeRootDescriptorTable(3, srvDescGPUHandle);
				commandList->SetComputeRootUnorderedAccessView(4, m_outputBuffer->GetGPUVirtualAddress());
				commandList->SetComputeRoot32BitConstant(5, m_width, 0);
				
				commandList->Dispatch(m_height, 1, 1);

				// Schedule to copy the data to the default buffer to the readback buffer.
				commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_outputBuffer.Get(),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE));

				commandList->CopyResource(m_readBackBuffer.Get(), m_outputBuffer.Get());

				commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_outputBuffer.Get(),
					D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
			}

			// Switch to different Color Mode.
			switch (m_appGui->GetAppData()->_EVisualizationColorMode)
			{
			case VCM_ColorWhite:
				commandList->SetPipelineState(m_PSOs["FullSQuadWhite"].Get());
				break;
			case VCM_ColorRGB:
				commandList->SetPipelineState(m_PSOs["FullSQuadRGB"].Get());
				break;
			default:
				break;
			}

			// Draw fullscreen quad to backbuffer (sample from offscreen RT).
			commandList->OMSetRenderTargets(1, &m_deviceResources->GetActiveRenderTargetView(), FALSE, 
				&m_deviceResources->GetActiveDepthStencilView());
			DrawFullscreenQuad(commandList);

			// State synchronization.
			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
				m_deviceResources->GetOffscreenRenderTarget(0),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				D3D12_RESOURCE_STATE_RENDER_TARGET));
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

void AppEntry::DrawFullscreenQuad(ID3D12GraphicsCommandList* commandList)
{
	// Null-out IA stage since we build the vertex off the SV_VertexID in the shader.
	commandList->IASetVertexBuffers(0, 1, nullptr);
	commandList->IASetIndexBuffer(nullptr);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->DrawInstanced(6, 1, 0, 0);
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
	if (m_bMousePosInBlockArea)
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
	if (m_bMousePosInBlockArea)
		return;
	ReleaseCapture();
}

void AppEntry::OnMouseMove(WPARAM btnState, int x, int y)
{
	if (CheckInBlockAreas(x, y))
		m_bMousePosInBlockArea = true;
	else m_bMousePosInBlockArea = false;

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

// x, y is not the coordinate of the cursor but pointer.
void AppEntry::OnMouseWheel(int d, WPARAM btnState, int x, int y)
{
	if (m_bMousePosInBlockArea)
		return;
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
	DXGI_FORMAT formats[] = { DXGI_FORMAT_R32G32B32A32_FLOAT };
	D3D12_RESOURCE_STATES defaultStates[] = { D3D12_RESOURCE_STATE_RENDER_TARGET };
	m_deviceResources->CreateOffscreenRenderTargets(_countof(formats), formats, defaultStates);

	CD3DX12_CPU_DESCRIPTOR_HANDLE srvDescCPUHandle(
		m_srvCbvDescHeap->GetCPUDescriptorHandleForHeapStart(), 1, 
		m_deviceResources->GetCbvSrvUavDescriptorSize());
	m_deviceResources->CreateTex2DShaderResourceView(m_deviceResources->GetOffscreenRenderTarget(0), srvDescCPUHandle, formats[0]);

	auto device = m_deviceResources->GetD3DDevice();

	// ComputeMax CS related Resources.
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(Math::Vector4)*m_height, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(&m_outputBuffer)));

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(Math::Vector4)*m_height),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_readBackBuffer)));
}

void AppEntry::BuildDescriptorHeaps()
{
	m_deviceResources->CreateCommonDescriptorHeap(m_appGui->GetDescriptorCount() + 1, &m_srvCbvDescHeap);
	m_appGui->SetSrvDescHeap(m_srvCbvDescHeap.Get());
}

void AppEntry::BuildConstantBuffers()
{
	
}

void AppEntry::BuildRootSignature()
{
	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

	const unsigned int NUM_ROOTPARAMETER = 6;
	CD3DX12_ROOT_PARAMETER slotRootParameter[NUM_ROOTPARAMETER];

	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstantBufferView(1);
	slotRootParameter[2].InitAsShaderResourceView(0);
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_ALL);
	slotRootParameter[4].InitAsUnorderedAccessView(0);
	slotRootParameter[5].InitAsConstants(1, 2); // Offscreen Width.

	// Because of indirect reference to sampler, we need to save it until our works finished.
	auto staticSampler = m_deviceResources->GetStaticSamplers(SS_PointClamp);
	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(NUM_ROOTPARAMETER, slotRootParameter, 1, &staticSampler,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	m_deviceResources->CreateRootSignature(&rootSigDesc, &m_ROOTSIGs["Main"]);

	// NOTE: Shader programs typically require resources as input (constant buffers,
	// textures, samplers).  The root signature defines the resources the shader
	// programs expect.  If we think of the shader programs as a function, and
	// the input resources as function parameters, then the root signature can be
	// thought of as defining the function signature.  
}

void AppEntry::BuildShadersAndInputLayout()
{
	m_shaderByteCode["VS"] = AppUtil::CompileShader(m_appPath + L"Shaders/common.hlsl", nullptr, "VS", "vs_5_0");	
	m_shaderByteCode["PS"] = AppUtil::CompileShader(m_appPath + L"Shaders/common.hlsl", nullptr, "PS", "ps_5_0");

	m_shaderByteCode["BoxVS"] = AppUtil::CompileShader(m_appPath + L"Shaders/common.hlsl", nullptr, "BoxVS", "vs_5_0");

	m_shaderByteCode["FullSQuadVS"] = AppUtil::CompileShader(m_appPath + L"Shaders/fullscreenquad.hlsl", nullptr, "VS", "vs_5_0");
	m_shaderByteCode["FullSQuadPS"] = AppUtil::CompileShader(m_appPath + L"Shaders/fullscreenquad.hlsl", nullptr, "PS", "ps_5_0");
	m_shaderByteCode["FullSQuadRGBPS"] = AppUtil::CompileShader(m_appPath + L"Shaders/fullscreenquad.hlsl", nullptr, "RGBPS", "ps_5_0");

	m_shaderByteCode["ComputeMaxCS"] = AppUtil::CompileShader(m_appPath + L"Shaders/computemax.hlsl", nullptr, "CS", "cs_5_0");

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

	auto fSceneRItem = std::make_unique<RenderItem>();
	fSceneRItem->Name = "box" + std::to_string(fSceneRItem->ObjectCBufferIndex);
	fSceneRItem->World = Matrix4(AffineTransform::MakeScale(m_appGui->GetAppData()->FSceneScale));
	fSceneRItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	fSceneRItem->PerFSceneSBufferOffset = (int)m_perFSceneCPUSBuffer.size();
	fSceneRItem->CreateCommonGeometry<ColorVertex, uint16>(m_deviceResources.get(), fSceneRItem->Name, fSceneMesh.Vertices, fSceneMesh.GetIndices16());

	m_renderItemLayer[RenderLayer::FScene].push_back(fSceneRItem.get());
	m_allRitems.push_back(std::move(fSceneRItem));
}

void AppEntry::BuildPSO()
{
	bool enable4xMsaa = m_deviceResources->GetDeviceOptions() & DeviceResources::c_Enable4xMsaa;

	//////////////////////////////////////////////////////////////////////////
	// PSO Default.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc0;
	ZeroMemory(&psoDesc0, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	/// Common.
	psoDesc0.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
	psoDesc0.pRootSignature = m_ROOTSIGs["Main"].Get();
	/// Shader.
	psoDesc0.VS =
	{
		reinterpret_cast<BYTE*>(m_shaderByteCode["VS"]->GetBufferPointer()),
		m_shaderByteCode["VS"]->GetBufferSize()
	};
	psoDesc0.PS =
	{
		reinterpret_cast<BYTE*>(m_shaderByteCode["PS"]->GetBufferPointer()),
		m_shaderByteCode["PS"]->GetBufferSize()
	};
	/// Default.
	psoDesc0.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc0.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc0.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc0.SampleMask = UINT_MAX;
	psoDesc0.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc0.NumRenderTargets = 1;
	psoDesc0.RTVFormats[0] = m_backBufferFormat;
	psoDesc0.SampleDesc.Count =  enable4xMsaa ? 4 : 1;
	psoDesc0.SampleDesc.Quality = enable4xMsaa ? (m_deviceResources->GetNum4MSQualityLevels() - 1) : 0;
	psoDesc0.DSVFormat = m_depthStencilFormat;
	m_deviceResources->CreateGraphicsPipelineState(&psoDesc0, &m_PSOs["Default"]);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// PSO Line.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc1 = psoDesc0;
	psoDesc1.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	m_deviceResources->CreateGraphicsPipelineState(&psoDesc1, &m_PSOs["Line"]);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// PSO BoxBlendAdd.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc2 = psoDesc0;
	psoDesc2.VS = 
	{
		reinterpret_cast<BYTE*>(m_shaderByteCode["BoxVS"]->GetBufferPointer()),
		m_shaderByteCode["BoxVS"]->GetBufferSize()
	};
	psoDesc2.DepthStencilState.DepthEnable = false;
	psoDesc2.BlendState.RenderTarget[0].BlendEnable = true;
	psoDesc2.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	psoDesc2.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
	m_deviceResources->CreateGraphicsPipelineState(&psoDesc2, &m_PSOs["BoxBlendAdd"]);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// PSO FullSQuadWhite.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc3 = psoDesc0;
	psoDesc3.VS =
	{
		reinterpret_cast<BYTE*>(m_shaderByteCode["FullSQuadVS"]->GetBufferPointer()),
		m_shaderByteCode["FullSQuadVS"]->GetBufferSize()
	};
	psoDesc3.PS =
	{
		reinterpret_cast<BYTE*>(m_shaderByteCode["FullSQuadPS"]->GetBufferPointer()),
		m_shaderByteCode["FullSQuadPS"]->GetBufferSize()
	};
	m_deviceResources->CreateGraphicsPipelineState(&psoDesc3, &m_PSOs["FullSQuadWhite"]);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// PSO FullSQuadRGB.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc4 = psoDesc3;
	psoDesc4.PS =
	{
		reinterpret_cast<BYTE*>(m_shaderByteCode["FullSQuadRGBPS"]->GetBufferPointer()),
		m_shaderByteCode["FullSQuadRGBPS"]->GetBufferSize()
	};
	m_deviceResources->CreateGraphicsPipelineState(&psoDesc4, &m_PSOs["FullSQuadRGB"]);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// PSO ComputeMax.
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoCSDesc0 = {};
	psoCSDesc0.pRootSignature = m_ROOTSIGs["Main"].Get();
	psoCSDesc0.CS =
	{
		reinterpret_cast<BYTE*>(m_shaderByteCode["ComputeMaxCS"]->GetBufferPointer()),
		m_shaderByteCode["ComputeMaxCS"]->GetBufferSize()
	};
	psoCSDesc0.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	m_deviceResources->CreateComputePipelineState(&psoCSDesc0, &m_PSOs["ComputeMax"]);
	//////////////////////////////////////////////////////////////////////////
}

#pragma endregion

#pragma region Interface IDeviceNotify
// IDeviceNotify
void AppEntry::OnDeviceLost()
{
    // TODO: Add Direct3D resource cleanup here.
	m_outputBuffer.Reset();
	m_readBackBuffer.Reset();
	m_srvCbvDescHeap.Reset();
	for (auto& e : m_ROOTSIGs)
		e.second.Reset();
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

void AppEntry::RenderGUI()
{
	m_appGui->RenderGUI();
}

void AppEntry::PostProcessing()
{

}

void AppEntry::OnOptionsChanged()
{
	if (m_deviceResources->GetDeviceOptions() & DeviceResources::c_Enable4xMsaa)
	{
		// 4xMsaa.
		BuildPSO();
	}
}

void AppEntry::ParseCommandLine(std::wstring cmdLine)
{

}

#pragma endregion
