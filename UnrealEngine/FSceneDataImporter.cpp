//
// FSceneDataImporter.cpp
// 

#include "FSceneDataImporter.h"
#include "../Common/FileManager.h"
#include "../Common/StringManager.h"

using namespace UnrealEngine;
using namespace DX::FileManager;
using namespace DX::StringManager;

void FSceneDataImporter::FillDataSets(const std::wstring& path)
{
	m_perLODDataSets.clear();

	std::wstring::size_type found = path.rfind(L"\\");
	// std::wstring path_prefix = path.substr(0, found);
	std::wstring file_prefix = path.substr(found + 1);

	// delete "World_".
	std::wstring dir_prefix = L"World_";
	found = file_prefix.find(dir_prefix);
	if (found != std::wstring::npos)
		file_prefix.erase(found, dir_prefix.length());

	file_prefix += L"_";

	//////////////////...COPY...////////////////////
	/*
	std::vector<std::wstring> search_tables =
	{
		L"StaticMeshesTable",		// LOD Yes.
		L"SkeletalMeshesTable",		// LOD Yes.
		L"LandscapesTable",			// Current Empty.
		L"PrimitiveTransforms",

		L"BoundsTable",
		L"MaterialsTable",
		L"MaterialInstancesTable",
		L"TexturesTable",

		L"LightMapsAndShadowMaps"
	};
	*/
	//////////////////...COPY...////////////////////

	std::vector<std::wstring> all_possible_files;

	FileUtil::WGetAllFilesUnder(path, all_possible_files, L".csv");

	// Calculate MAX_LOD.
	int32 max_lod = 0;
	std::wstring postfix = L"_LOD";
	for (auto& file : all_possible_files)
	{
		std::wstring::size_type found = file.rfind(postfix);
		if (found != std::wstring::npos)
			max_lod = DirectX::XMMax<int32>(max_lod, StringUtil::WCharToInt32(file[found + postfix.size()]));
	}

	m_perLODDataSets.resize(max_lod + 1);

	// read in .csv files.
	std::unordered_map <std::wstring, std::vector<std::wstring>> g_tables;

	for (auto& _file : all_possible_files)
	{
		std::wifstream fin;
		std::wstring dir = path + L"\\";
		std::wstring result;
		std::vector<std::wstring> table;

		fin.open(dir + _file);
		found = std::wstring::npos;
		if (fin.good())
		{
			while (!fin.eof())
			{
				std::getline(fin, result);
				table.push_back(result);
			}
		}
		fin.close();
		table.erase(table.begin());

		std::wstring table_name = _file;
		found = table_name.find(file_prefix);
		table_name.erase(found, file_prefix.size());
		found = table_name.rfind(L".csv");
		table_name.erase(found, 4);
		g_tables[table_name] = table;
	}

	// extract data to clear structure for use.
	std::unordered_map <std::wstring, std::vector<std::vector<std::wstring>>> g_clear_tables;

	for (auto& _table : g_tables)
	{
		std::vector<std::vector<std::wstring>> clear_table;
		for (auto& _row : _table.second)
		{
			if (_row.empty())
				continue;
			std::vector<std::wstring> items;
			std::wstring::size_type last_found = 0;
			found = _row.find_first_of(L",\"");
			while (found != std::wstring::npos)
			{
				if (_row.substr(found, 1) == L",")
				{
					items.push_back(_row.substr(last_found, found - last_found));
					last_found = found + 1;
				}
				else if (_row.substr(found, 1) == L"\"")
				{
					items.push_back(StringUtil::WFindFirstBetween(_row, _row.substr(found, 1), last_found, last_found));
					last_found++; // skip the next ','.
				}
				found = _row.find_first_of(L",\"", last_found);
			}

			items.push_back(_row.substr(last_found, _row.size() - last_found)); // the last item.

			clear_table.push_back(items);
		}

		g_clear_tables[_table.first] = clear_table;
	}

	// fill data sets.
	_FillDataSets(g_clear_tables);
}

#define safe_index(row, index) row[index++];if (index >= row.size()) break;
#define numeric_safe_index(row, index, type) StringUtil::WStringToNumeric<type>(row[index++]);if (index >= row.size()) break;
#define array_safe_index(row, index, type, separator) StringUtil::WStringToArray<type>(row[index++],separator);if (index >= row.size()) break;
#define array_first_safe_index(row, index, type, separator) StringUtil::WStringToArray<type>(row[index++],separator).front();if (index >= row.size()) break;

void FSceneDataImporter::_FillDataSets(std::unordered_map <std::wstring, std::vector<std::vector<std::wstring>>>& clear_tables)
{
	FSceneStaticMeshDataSet			StaticMeshDataSet;
	FSceneSkeletalMeshDataSet		SkeletalMeshDataSet;
	// FSceneLandscapeDataSet		LandscapeDataSet;		// Current Empty.
	FMatrix							PrimitiveTransform;
	FBoxSphereBounds				Bounds;
	FSceneMaterialDataSet			MaterialsDataSet;
	FSceneMaterialInstanceDataSet	MaterialInstancesDataSet;
	FSceneTextureDataSet			TexturesDataSet;
	FSceneTextureDataSet			LightMapsAndShadowMaps;

	std::vector<std::vector<std::wstring>> table;

	for (int i = 0; i < m_perLODDataSets.size(); ++i)
	{
		table = clear_tables[L"StaticMeshesTable_LOD" + std::to_wstring(i)];
		if (!table.empty())
		{
			for (auto& row : table)
			{
				while (true) // This 'while' will be 'break' automatically.
				{
					int index = 1; // Id is Discarded.
					StaticMeshDataSet.Name = safe_index(row, index);
					StaticMeshDataSet.OwnerName = safe_index(row, index);
					StaticMeshDataSet.NumVertices = numeric_safe_index(row, index, uint32);
					StaticMeshDataSet.NumTriangles = numeric_safe_index(row, index, uint32);
					StaticMeshDataSet.NumInstances = numeric_safe_index(row, index, uint32);
					StaticMeshDataSet.NumLODs = numeric_safe_index(row, index, uint16);
					StaticMeshDataSet.CurrentLOD = numeric_safe_index(row, index, uint16);
					StaticMeshDataSet.AssetPath = safe_index(row, index);
					StaticMeshDataSet.UniqueId = numeric_safe_index(row, index, uint32);
					StaticMeshDataSet.BoundsIndices = array_safe_index(row, index, int32, L'\\');
					StaticMeshDataSet.TransformsIndices = array_safe_index(row, index, int32, L'\\');
					StaticMeshDataSet.UsedMaterialsIndices = array_safe_index(row, index, int32, L'\\');
					StaticMeshDataSet.UsedMaterialIntancesIndices = array_safe_index(row, index, int32, L'\\');
				}

				m_perLODDataSets[i].StaticMeshesTable.push_back(StaticMeshDataSet);
			}
		}

		table.clear();
		table = clear_tables[L"SkeletalMeshesTable_LOD" + std::to_wstring(i)];
		if (!table.empty())
		{
			for (auto& row : table)
			{
				while (true) // This 'while' will be 'break' automatically.
				{
					int index = 1; // Id is Discarded.
					SkeletalMeshDataSet.Name = safe_index(row, index);
					SkeletalMeshDataSet.OwnerName = safe_index(row, index);
					SkeletalMeshDataSet.NumVertices = numeric_safe_index(row, index, uint32);
					SkeletalMeshDataSet.NumTriangles = numeric_safe_index(row, index, uint32);
					SkeletalMeshDataSet.NumSections = numeric_safe_index(row, index, uint32);
					SkeletalMeshDataSet.NumLODs = numeric_safe_index(row, index, uint16);
					SkeletalMeshDataSet.CurrentLOD = numeric_safe_index(row, index, uint16);
					SkeletalMeshDataSet.AssetPath = safe_index(row, index);
					SkeletalMeshDataSet.UniqueId = numeric_safe_index(row, index, uint32);
					SkeletalMeshDataSet.BoundsIndex = array_first_safe_index(row, index, int32, L'\\');
					SkeletalMeshDataSet.TransformsIndex = array_first_safe_index(row, index, int32, L'\\');
					SkeletalMeshDataSet.UsedMaterialsIndices = array_safe_index(row, index, int32, L'\\');
					SkeletalMeshDataSet.UsedMaterialIntancesIndices = array_safe_index(row, index, int32, L'\\');
				}

				m_perLODDataSets[i].SkeletalMeshesTable.push_back(SkeletalMeshDataSet);
			}
		}

		table.clear();
		table = clear_tables[L"PrimitiveTransforms_LOD" + std::to_wstring(i)];
		if (!table.empty())
		{
			for (auto& row : table)
			{
				while (true) // This 'while' will be 'break' automatically.
				{
					int index = 1; // Id is Discarded.

					float m_00 = numeric_safe_index(row, index, float);
					float m_01 = numeric_safe_index(row, index, float);
					float m_02 = numeric_safe_index(row, index, float);
					float m_03 = numeric_safe_index(row, index, float);

					float m_10 = numeric_safe_index(row, index, float);
					float m_11 = numeric_safe_index(row, index, float);
					float m_12 = numeric_safe_index(row, index, float);
					float m_13 = numeric_safe_index(row, index, float);

					float m_20 = numeric_safe_index(row, index, float);
					float m_21 = numeric_safe_index(row, index, float);
					float m_22 = numeric_safe_index(row, index, float);
					float m_23 = numeric_safe_index(row, index, float);

					float m_30 = numeric_safe_index(row, index, float);
					float m_31 = numeric_safe_index(row, index, float);
					float m_32 = numeric_safe_index(row, index, float);
					float m_33 = numeric_safe_index(row, index, float);

					PrimitiveTransform = FMatrix(
						Vector4(m_00, m_01, m_02, m_03),
						Vector4(m_10, m_11, m_12, m_13),
						Vector4(m_20, m_21, m_22, m_23),
						Vector4(m_30, m_31, m_32, m_33));
				}

				m_perLODDataSets[i].PrimitiveTransforms.push_back(PrimitiveTransform);
			}
		}

		table.clear();
		table = clear_tables[L"BoundsTable_LOD" + std::to_wstring(i)];
		if (!table.empty())
		{
			for (auto& row : table)
			{
				while (true) // This 'while' will be 'break' automatically.
				{
					int index = 1; // Id is Discarded.

					Bounds.Origin.x = numeric_safe_index(row, index, float);
					Bounds.Origin.y = numeric_safe_index(row, index, float);
					Bounds.Origin.z = numeric_safe_index(row, index, float);

					Bounds.BoxExtent.x = numeric_safe_index(row, index, float);
					Bounds.BoxExtent.y = numeric_safe_index(row, index, float);
					Bounds.BoxExtent.z = numeric_safe_index(row, index, float);

					Bounds.SphereRadius = numeric_safe_index(row, index, float);
				}
				Bounds.BoxBounds.Center = Bounds.Origin;
				Bounds.BoxBounds.Extents = Bounds.BoxExtent;

				Bounds.SphereBounds.Center = Bounds.Origin;
				Bounds.SphereBounds.Radius = Bounds.SphereRadius;

				m_perLODDataSets[i].BoundsTable.push_back(Bounds);
			}
		}

		table.clear();
		table = clear_tables[L"MaterialsTable_LOD" + std::to_wstring(i)];
		if (!table.empty())
		{
			for (auto& row : table)
			{
				while (true) // This 'while' will be 'break' automatically.
				{
					int index = 1; // Id is Discarded.
					MaterialsDataSet.Name = safe_index(row, index);
					MaterialsDataSet.NumInstances = numeric_safe_index(row, index, uint32);
					MaterialsDataSet.NumRefs = numeric_safe_index(row, index, uint32);
					MaterialsDataSet.BPSCount = numeric_safe_index(row, index, int32);
					MaterialsDataSet.BPSSurfaceLightmap = numeric_safe_index(row, index, int32);
					MaterialsDataSet.BPSVolumetricLightmap = numeric_safe_index(row, index, int32);
					MaterialsDataSet.BPSVertex = numeric_safe_index(row, index, int32);
					MaterialsDataSet.TexSamplers = safe_index(row, index);
					MaterialsDataSet.UserInterpolators = safe_index(row, index);
					MaterialsDataSet.TexLookups = safe_index(row, index);
					MaterialsDataSet.VTLookups = safe_index(row, index);
					MaterialsDataSet.ShaderErrors = safe_index(row, index);
					MaterialsDataSet.MaterialDomain = safe_index(row, index);
					MaterialsDataSet.BlendMode = safe_index(row, index);
					MaterialsDataSet.DecalBlendMode = safe_index(row, index);
					MaterialsDataSet.ShadingModel = safe_index(row, index);
					MaterialsDataSet.TwoSided = (uint8)numeric_safe_index(row, index, uint16);
					MaterialsDataSet.bCastRayTracedShadows = (uint8)numeric_safe_index(row, index, uint16);
					MaterialsDataSet.bScreenSpaceReflections = (uint8)numeric_safe_index(row, index, uint16);
					MaterialsDataSet.bContactShadows = (uint8)numeric_safe_index(row, index, uint16);
					MaterialsDataSet.TranslucencyLightingMode = safe_index(row, index);
					MaterialsDataSet.TranslucencyDirectionalLightingIntensity = numeric_safe_index(row, index, float);
					MaterialsDataSet.bUseTranslucencyVertexFog = (uint8)numeric_safe_index(row, index, uint16);
					MaterialsDataSet.bComputeFogPerPixel = (uint8)numeric_safe_index(row, index, uint16);
					MaterialsDataSet.bOutputTranslucentVelocity = (uint8)numeric_safe_index(row, index, uint16);
					MaterialsDataSet.bEnableSeparateTranslucency = (uint8)numeric_safe_index(row, index, uint16);
					MaterialsDataSet.bEnableResponsiveAA = (uint8)numeric_safe_index(row, index, uint16);
					MaterialsDataSet.bEnableMobileSeparateTranslucency = (uint8)numeric_safe_index(row, index, uint16);
					MaterialsDataSet.bDisableDepthTest = (uint8)numeric_safe_index(row, index, uint16);
					MaterialsDataSet.bWriteOnlyAlpha = (uint8)numeric_safe_index(row, index, uint16);
					MaterialsDataSet.AllowTranslucentCustomDepthWrites = (uint8)numeric_safe_index(row, index, uint16);
					MaterialsDataSet.bUseFullPrecision = (uint8)numeric_safe_index(row, index, uint16);
					MaterialsDataSet.bUseLightmapDirectionality = (uint8)numeric_safe_index(row, index, uint16);
					MaterialsDataSet.bUseHQForwardReflections = (uint8)numeric_safe_index(row, index, uint16);
					MaterialsDataSet.bUsePlanarForwardReflections = (uint8)numeric_safe_index(row, index, uint16);
					MaterialsDataSet.AssetPath = safe_index(row, index);
					MaterialsDataSet.UniqueId = numeric_safe_index(row, index, uint32);
					MaterialsDataSet.UsedTexturesIndices = array_safe_index(row, index, int32, L'\\');
					MaterialsDataSet.MatInsIndices = array_safe_index(row, index, int32, L'\\');
				}

				m_perLODDataSets[i].MaterialsTable.push_back(MaterialsDataSet);
			}
		}

		table.clear();
		table = clear_tables[L"MaterialInstancesTable_LOD" + std::to_wstring(i)];
		if (!table.empty())
		{
			for (auto& row : table)
			{
				while (true) // This 'while' will be 'break' automatically.
				{
					int index = 1; // Id is Discarded.
					MaterialInstancesDataSet.Name = safe_index(row, index);
					MaterialInstancesDataSet.NumRefs = numeric_safe_index(row, index, uint32);
					MaterialInstancesDataSet.ParentName = safe_index(row, index);
					MaterialInstancesDataSet.ParentIndex = numeric_safe_index(row, index, int32);
					MaterialInstancesDataSet.AssetPath = safe_index(row, index);
					MaterialInstancesDataSet.UniqueId = numeric_safe_index(row, index, uint32);
					MaterialInstancesDataSet.UsedTexturesIndices = array_safe_index(row, index, int32, L'\\');
				}

				m_perLODDataSets[i].MaterialInstancesTable.push_back(MaterialInstancesDataSet);
			}
		}

		table.clear();
		table = clear_tables[L"TexturesTable_LOD" + std::to_wstring(i)];
		if (!table.empty())
		{
			for (auto& row : table)
			{
				while (true) // This 'while' will be 'break' automatically.
				{
					int index = 1; // Id is Discarded.
					TexturesDataSet.Name = safe_index(row, index);
					TexturesDataSet.Type = safe_index(row, index);
					TexturesDataSet.NumRefs = numeric_safe_index(row, index, uint32);
					TexturesDataSet.CurrentSize = safe_index(row, index);
					TexturesDataSet.PixelFormat = safe_index(row, index);
					TexturesDataSet.CurrentKB = numeric_safe_index(row, index, float);
					TexturesDataSet.FullyLoadedKB = numeric_safe_index(row, index, float);
					TexturesDataSet.PVRTC2 = numeric_safe_index(row, index, float);
					TexturesDataSet.PVRTC4 = numeric_safe_index(row, index, float);
					TexturesDataSet.ASTC_4x4 = numeric_safe_index(row, index, float);
					TexturesDataSet.ASTC_6x6 = numeric_safe_index(row, index, float);
					TexturesDataSet.ASTC_8x8 = numeric_safe_index(row, index, float);
					TexturesDataSet.ASTC_10x10 = numeric_safe_index(row, index, float);
					TexturesDataSet.ASTC_12x12 = numeric_safe_index(row, index, float);
					TexturesDataSet.SourceSize = safe_index(row, index);
					TexturesDataSet.SourceFormat = safe_index(row, index);
					TexturesDataSet.CompressionNoAlpha = (uint8)numeric_safe_index(row, index, uint16);
					TexturesDataSet.LODBias = numeric_safe_index(row, index, int32);
					TexturesDataSet.NumResidentMips = (uint8)numeric_safe_index(row, index, uint16);
					TexturesDataSet.NumMipsAllowed = (uint8)numeric_safe_index(row, index, uint16);
					TexturesDataSet.CurrentMips = (uint8)numeric_safe_index(row, index, uint16);
					TexturesDataSet.CurrentSizeX = numeric_safe_index(row, index, uint16);
					TexturesDataSet.CurrentSizeY = numeric_safe_index(row, index, uint16);
					TexturesDataSet.SourceSizeX = numeric_safe_index(row, index, uint16);
					TexturesDataSet.SourceSizeY = numeric_safe_index(row, index, uint16);
					TexturesDataSet.AssetPath = safe_index(row, index);
					TexturesDataSet.UniqueId = numeric_safe_index(row, index, uint32);
				}

				m_perLODDataSets[i].TexturesTable.push_back(TexturesDataSet);
			}
		}

		table.clear();
		table = clear_tables[L"LightMapsAndShadowMaps"];
		if (!table.empty())
		{
			if (i > 0) continue;
			for (auto& row : table)
			{
				while (true) // This 'while' will be 'break' automatically.
				{
					int index = 1; // Id is Discarded.
					LightMapsAndShadowMaps.Name = safe_index(row, index);
					LightMapsAndShadowMaps.Type = safe_index(row, index);
					LightMapsAndShadowMaps.NumRefs = numeric_safe_index(row, index, uint32);
					LightMapsAndShadowMaps.CurrentSize = safe_index(row, index);
					LightMapsAndShadowMaps.PixelFormat = safe_index(row, index);
					LightMapsAndShadowMaps.CurrentKB = numeric_safe_index(row, index, float);
					LightMapsAndShadowMaps.FullyLoadedKB = numeric_safe_index(row, index, float);
					LightMapsAndShadowMaps.PVRTC2 = numeric_safe_index(row, index, float);
					LightMapsAndShadowMaps.PVRTC4 = numeric_safe_index(row, index, float);
					LightMapsAndShadowMaps.ASTC_4x4 = numeric_safe_index(row, index, float);
					LightMapsAndShadowMaps.ASTC_6x6 = numeric_safe_index(row, index, float);
					LightMapsAndShadowMaps.ASTC_8x8 = numeric_safe_index(row, index, float);
					LightMapsAndShadowMaps.ASTC_10x10 = numeric_safe_index(row, index, float);
					LightMapsAndShadowMaps.ASTC_12x12 = numeric_safe_index(row, index, float);
					LightMapsAndShadowMaps.SourceSize = safe_index(row, index);
					LightMapsAndShadowMaps.SourceFormat = safe_index(row, index);
					LightMapsAndShadowMaps.CompressionNoAlpha = (uint8)numeric_safe_index(row, index, uint16);
					LightMapsAndShadowMaps.LODBias = numeric_safe_index(row, index, int32);
					LightMapsAndShadowMaps.NumResidentMips = (uint8)numeric_safe_index(row, index, uint16);
					LightMapsAndShadowMaps.NumMipsAllowed = (uint8)numeric_safe_index(row, index, uint16);
					LightMapsAndShadowMaps.CurrentMips = (uint8)numeric_safe_index(row, index, uint16);
					LightMapsAndShadowMaps.CurrentSizeX = numeric_safe_index(row, index, uint16);
					LightMapsAndShadowMaps.CurrentSizeY = numeric_safe_index(row, index, uint16);
					LightMapsAndShadowMaps.SourceSizeX = numeric_safe_index(row, index, uint16);
					LightMapsAndShadowMaps.SourceSizeY = numeric_safe_index(row, index, uint16);
					LightMapsAndShadowMaps.AssetPath = safe_index(row, index);
					LightMapsAndShadowMaps.UniqueId = numeric_safe_index(row, index, uint32);
				}

				m_perLODDataSets[i].LightMapsAndShadowMaps.push_back(LightMapsAndShadowMaps);
			}
		}
	}
}

const FSceneDataSet* FSceneDataImporter::GetFSceneData(int lod) const
{
	if (lod < m_perLODDataSets.size())
		return &m_perLODDataSets[lod];
	else return nullptr;
}
