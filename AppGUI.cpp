//
// AppGUI.h
//

#include "AppGUI.h"
#include "imgui/imgui.h"
#include "imgui/impl/imgui_impl_win32.h"
#include "imgui/impl/imgui_impl_dx12.h"
#include "Common/FileManager.h"
#include "Common/StringManager.h"

using namespace DX::StringManager;

AppGUI::AppGUI(Window InWindow, ID3D12Device* InDevice, ID3D12GraphicsCommandList* InCmdList, UINT InNumFrameResources)
	: m_window(InWindow), m_d3dDevice(InDevice), m_d3dCommandList(InCmdList), m_numFrameResources(InNumFrameResources)
{
	// ...
	m_descIncrementSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	m_appData = std::make_unique<AppData>();
	m_importer = std::make_unique<FSceneDataImporter>();
}

AppGUI::~AppGUI()
{
	// ...
	m_d3dDevice = nullptr;
	m_d3dCommandList = nullptr;
	m_d3dSrvDescHeap = nullptr;
}

bool AppGUI::InitGUI(std::wstring cmdLine)
{
	ParseCommandLine(cmdLine);

	// Setup Dear ImGui context.
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	
	// Save imgui.ini manually.
	io.IniFilename = NULL;
	ImGui::LoadIniSettingsFromDisk(std::string(StringUtil::WStringToString(m_appData->AppPath) + "AppGui.ini").c_str());

	// Setup Dear ImGui style.
	ImGui::StyleColorsClassic();

	// Setup Platform/Renderer bindings.
	if (!ImGui_ImplWin32_Init(m_window.Handle))
	{
		return false;
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE srvCpuDescHandle(m_d3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart());	
	CD3DX12_GPU_DESCRIPTOR_HANDLE srvGpuDescHandle(m_d3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());

	if (m_offsetInDescs != 0)
	{
		srvCpuDescHandle.Offset(m_offsetInDescs, m_descIncrementSize);
		srvGpuDescHandle.Offset(m_offsetInDescs, m_descIncrementSize);
	}	

	if (!ImGui_ImplDX12_Init(m_d3dDevice, m_numFrameResources,
		DXGI_FORMAT_R8G8B8A8_UNORM, m_d3dSrvDescHeap,
		srvCpuDescHandle,
		srvGpuDescHandle))
	{
		return false;
	}

	// Load Fonts.
	ImFont* font = io.Fonts->AddFontFromFileTTF(
		"C:/Windows/Fonts/msyh.ttc",
		20.0f, NULL, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
	IM_ASSERT(font != NULL);

	return true;
}

void AppGUI::RenderGUI()
{
	NewFrame();
	DrawGUI();
	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_d3dCommandList);
}

void AppGUI::DestroyGUI()
{
	// After WaitForGpu.
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::SaveIniSettingsToDisk(std::string(StringUtil::WStringToString(m_appData->AppPath) + "AppGui.ini").c_str());
	ImGui::DestroyContext();
}

void AppGUI::GUIInvalidateDeviceObjects()
{
	return ImGui_ImplDX12_InvalidateDeviceObjects(); // void
}

bool AppGUI::GUICreateDeviceObjects()
{
	return ImGui_ImplDX12_CreateDeviceObjects();
}

void AppGUI::SetSrvDescHeap(ID3D12DescriptorHeap* InSrvDescHeap, INT InOffset)
{
	m_d3dSrvDescHeap = InSrvDescHeap;
	m_offsetInDescs = InOffset;
}

// Win32 message handler
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT AppGUI::GUIMessageProcessor(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
}

void AppGUI::NewFrame()
{
	// Start the Dear ImGui frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void AppGUI::DrawGUI()
{
	m_appData->BlockAreas.resize(2);
	// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
	// if (m_bShowDemo)
		// ImGui::ShowDemoWindow(&m_bShowDemo);

	// 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
	{
		if (!ImGui::Begin(u8"控制面板"))							// Create a window called "Hello, world!" and append into it.
			ImGui::End();											// Early out if the window is collapsed, as an optimization.
		else
		{
			SetBlockAreas(0);
			
			// ImGui::Checkbox("Demo Window", &m_bShowDemo);			// Edit bools storing our window open/close state
			// if (ImGui::Checkbox("Enable4xMsaa", &m_appData->bEnable4xMsaa))
				// m_appData->bOptionsChanged = true;
			ImGui::Text(u8"摄像机属性");

			const char* cameraViewTypes[] = 
			{ 
				NameOf(CV_FirstPersonView), 
				NameOf(CV_FocusPointView), 
				NameOf(CV_TopView),
				NameOf(CV_BottomView),
				NameOf(CV_LeftView),
				NameOf(CV_RightView),
				NameOf(CV_FrontView),
				NameOf(CV_BackView)
			};
			ImGui::Combo(u8"视图类型", &(int)m_appData->_ECameraViewType, cameraViewTypes, IM_ARRAYSIZE(cameraViewTypes));

			const char* cameraProjTypes[] =
			{
				NameOf(CP_PerspectiveProj),
				NameOf(CP_OrthographicProj)
			};
			ImGui::Combo(u8"投影类型", &(int)m_appData->_ECameraProjType, cameraProjTypes, IM_ARRAYSIZE(cameraProjTypes));

			if (ImGui::DragFloat(u8"FarZ", &m_appData->CameraFarZ, 1.0f, 100.0f, 2000.0f))
				m_appData->bCameraFarZDirty = true;

			const char* visualizationColorModes[] =
			{
				NameOf(VCM_ColorWhite),
				NameOf(VCM_ColorRGB)
			};
			ImGui::Combo(u8"颜色模式", &(int)m_appData->_EVisualizationColorMode, visualizationColorModes, IM_ARRAYSIZE(visualizationColorModes));

			ImGui::Checkbox(u8"显示参考网格", &m_appData->bShowGrid);
			if (m_appData->bShowGrid)
			{
				ImGui::SetNextItemWidth(70);
				if (ImGui::DragFloat(u8"长度", &m_appData->GridWidth, 1.0f, 20.0f, std::numeric_limits<float>::max()))
					m_appData->bGridDirdy = true;
				ImGui::SameLine();
				ImGui::SetNextItemWidth(70);
				if (ImGui::DragInt(u8"单元", &(int)m_appData->GridUnit, 20.0f, 20, 2000))
					m_appData->bGridDirdy = true;
				// ImGui::Checkbox(u8"更新", &m_appData->bGridDirdy);

				static Vector3 trans = { 0.0f };
				static Vector3 rotat = { 0.0f };
				static Vector3 scale = { 1.0f };

				ImGui::DragFloat3(u8"位置", (float*)&trans, 0.1f, -10.0f, 10.0f);
				ImGui::DragFloat3(u8"旋转", (float*)&rotat, 0.1f, -360.0f, 360.0f);
				ImGui::DragFloat3(u8"缩放", (float*)&scale, 0.1f, 0.0f, 10.0f);

				// m_appData->LocalToWorld = Matrix4(AffineTransform(Quaternion(rotat.GetX(), rotat.GetY(), rotat.GetZ()), trans)*AffineTransform::MakeScale(scale));
				m_appData->LocalToWorld = Matrix4(AffineTransform(trans).Rotation(rotat*(XM_PI/180.0f)).Scale(scale));			
			}
	
			ImGui::Separator();
			ImGui::ColorEdit3("clear color", (float*)&m_appData->ClearColor); // Edit 3 floats representing a color	
			ImGui::DragFloat(u8"输入场景比例", &m_appData->FSceneScale, 0.00001f, 0.0001f, 1000.0f, "%.5f");

			ImGui::Checkbox(u8"允许实时计算最大值", &m_appData->bEnableCalcMax);
			if (ImGui::Button(u8"设置"))
				m_appData->Overflow = m_appData->MaxPixel.GetX();
			ImGui::SameLine();
			ImGui::SetNextItemWidth(100);
			ImGui::InputFloat(u8"最大值", (float*)&m_appData->MaxPixel, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_ReadOnly);

			if (ImGui::CollapsingHeader(u8"可视化对象选择"))
			{
				int* va_ptr = &(int)m_appData->_EVisualizationAttribute;

#define GUI_SetAttribute(x) if (ImGui::RadioButton(NameOf(x), va_ptr, (int)x)) m_appData->bVisualizationAttributeDirty = true;

				if (ImGui::TreeNode("VisualizationAttribute"))
				{
					GUI_SetAttribute(VA_NumVertices);
					GUI_SetAttribute(VA_NumTriangles);
					GUI_SetAttribute(VA_NumInstances);
					GUI_SetAttribute(VA_NumLODs);
					GUI_SetAttribute(VA_NumMaterials);
					GUI_SetAttribute(VA_NumTextures);

					if (ImGui::TreeNode("Material"))
					{
						if (ImGui::TreeNode("Texture"))
						{
							GUI_SetAttribute(VA_CurrentKB);
							ImGui::TreePop();
						}

						GUI_SetAttribute(VA_UniformBufferSize);
						GUI_SetAttribute(VA_NumUniformBufferMembers);
						GUI_SetAttribute(VA_Stats_Base_Pass_Shader_Instructions);
						GUI_SetAttribute(VA_Stats_Base_Pass_Shader_With_Surface_Lightmap);
						GUI_SetAttribute(VA_Stats_Base_Pass_Shader_With_Volumetric_Lightmap);
						GUI_SetAttribute(VA_Stats_Base_Pass_Vertex_Shader);
						GUI_SetAttribute(VA_Stats_Texture_Samplers);
						GUI_SetAttribute(VA_Stats_User_Interpolators_Scalars);
						GUI_SetAttribute(VA_Stats_User_Interpolators_Vectors);
						GUI_SetAttribute(VA_Stats_User_Interpolators_TexCoords);
						GUI_SetAttribute(VA_Stats_User_Interpolators_Custom);
						GUI_SetAttribute(VA_Stats_Texture_Lookups_VS);
						GUI_SetAttribute(VA_Stats_Texture_Lookups_PS);
						GUI_SetAttribute(VA_Stats_Virtual_Texture_Lookups);
						GUI_SetAttribute(VA_Material_Two_Sided);
						GUI_SetAttribute(VA_Material_Cast_Ray_Traced_Shadows);
						GUI_SetAttribute(VA_Translucency_Screen_Space_Reflections);
						GUI_SetAttribute(VA_Translucency_Contact_Shadows);
						GUI_SetAttribute(VA_Translucency_Directional_Lighting_Intensity);
						GUI_SetAttribute(VA_Translucency_Apply_Fogging);
						GUI_SetAttribute(VA_Translucency_Compute_Fog_Per_Pixel);
						GUI_SetAttribute(VA_Translucency_Output_Velocity);
						GUI_SetAttribute(VA_Translucency_Render_After_DOF);
						GUI_SetAttribute(VA_Translucency_Responsive_AA);
						GUI_SetAttribute(VA_Translucency_Mobile_Separate_Translucency);
						GUI_SetAttribute(VA_Translucency_Disable_Depth_Test);
						GUI_SetAttribute(VA_Translucency_Write_Only_Alpha);
						GUI_SetAttribute(VA_Translucency_Allow_Custom_Depth_Writes);
						GUI_SetAttribute(VA_Mobile_Use_Full_Precision);
						GUI_SetAttribute(VA_Mobile_Use_Lightmap_Directionality);
						GUI_SetAttribute(VA_Forward_Shading_High_Quality_Reflections);
						GUI_SetAttribute(VA_Forward_Shading_Planar_Reflections);
						
						ImGui::TreePop();
					}
					ImGui::TreePop();
				}
			}

			ImGui::DragFloat("Speed", &m_appData->DragSpeed, 1.0f, 1.0f, 100.0f);
			ImGui::DragFloat("Overflow", &m_appData->Overflow, m_appData->DragSpeed, 1.0f, std::numeric_limits<float>::max());

			if (ImGui::Button(u8"导入UE4场景数据"))
			{
				std::wstring path;
				if (FileManager::FileUtil::OpenDialogBox(m_window.Handle, path, FOS_PICKFOLDERS))
				{
					ImportFSceneFromDir(path);					
				}
			}

			ImGui::SameLine();
			if (ImGui::Button(u8"清空"))
				m_appData->bClearFScene = true;

			ImGui::SameLine();
			ImGui::Text(u8"默认LOD0");

			if (m_notifyImporterBegin)
			{
				ImGui::OpenPopup(u8"导入器");
				m_notifyImporterBegin = false;
			}

			static std::string hint;
			if (m_importerLock)
			{
				m_performanceCounter.EndCounter("importer");
				hint = u8"文件加载完毕！\n用时：" + std::to_string(m_performanceCounter.GetCounterResult("importer")) + " s\n";
				ImGui::Text(hint.c_str());
			}
			else hint = u8"文件加载中，请稍后...\n";

			if (ImGui::BeginPopupModal(u8"导入器", NULL, ImGuiWindowFlags_AlwaysAutoResize))
			{
				SetBlockAreas(1, true);

				ImGui::Text(hint.c_str());
				ImGui::Separator();

				if (ImGui::Button(u8"确定", ImVec2(60, 0))) {
					ImGui::CloseCurrentPopup(); m_appData->BlockAreas[1] = nullptr;
				}						
			
				ImGui::EndPopup();
			}

			ImGui::Text(u8"Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::End();
		}		
	}
}

void AppGUI::ImportFSceneFromDir(std::wstring path)
{
	static std::wstring g_path;
	g_path = path;
	m_importerLock = false;
	m_notifyImporterBegin = true;
	m_performanceCounter.BeginCounter("importer");

	std::thread([&]()
	{
		if (!m_importerLock)
		{
			if (!m_importer->GetDataDirtyFlag())
			{
				m_importer->FillDataSets(g_path);
				m_importer->SetDataDirtyFlag(true);
			}
			m_importerLock = true;
		}

	}).detach();
}

void AppGUI::SetBlockAreas(int index, bool bFullScreen)
{
	if (index >= m_appData->BlockAreas.size())
		return;

	auto blockArea = std::make_unique<BlockArea>();
	if (bFullScreen)
	{
		auto blockArea = std::make_unique<BlockArea>();
		blockArea->StartPos = XMINT2(0, 0);
		blockArea->BlockSize = m_window.Size;
	}
	else
	{
		ImVec2 current_pos = ImGui::GetWindowPos();
		ImVec2 current_size = ImGui::GetWindowSize();
	
		blockArea->StartPos = XMINT2((int32)current_pos.x, (int32)current_pos.y);
		blockArea->BlockSize = XMINT2((int32)current_size.x, (int32)current_size.y);		
	}

	m_appData->BlockAreas[0] = std::move(blockArea);
}

void AppGUI::ParseCommandLine(std::wstring cmdLine)
{
	std::vector<std::wstring> cmds = StringUtil::WGetBetween(cmdLine, L"-dir [", L"]");
	if (!cmds.empty())
	{
		ImportFSceneFromDir(cmds.front());
	}
	cmds.clear();
	cmds = StringUtil::WGetBetween(cmdLine, L"-scale [", L"]");
	if (!cmds.empty())
	{
		m_appData->FSceneScale = StringUtil::WStringToNumeric<float>(cmds.front());
	}
}
