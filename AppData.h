//
// AppData.h
//

#pragma once

#include <DirectXMath.h>
#include <DirectXCollision.h>
#include "Common/TypeDef.h"
#include "Common/VectorMath.h"

using namespace DirectX;
using namespace Math;

namespace DX
{
	struct BlockArea
	{
		XMINT2 StartPos;
		XMINT2 BlockSize;
	};

	struct AppData
	{
		// User Data.
		Vector4 ClearColor = { 0.608f, 0.689f, 0.730f, 1.0f };
		Matrix4 LocalToWorld = Matrix4(EIdentityTag::kIdentity);
		bool bEnable4xMsaa = false;
		CameraViewType ECameraViewType = CV_FirstPersonView;
		CameraProjType ECameraProjType = CP_PerspectiveProj;
		float DragSpeed = 1.0f;
		float Overflow = 1000.0f;
		bool bShowGrid = true;

		// App Data.
		std::vector<std::unique_ptr<BlockArea>> BlockAreas;
		bool bOptionsChanged = false;
	};

	struct BoxSphereBounds
	{
	public:

		/** Holds the origin of the bounding box and sphere. */
		XMFLOAT3 Origin;

		/** Holds the extent of the bounding box. */
		XMFLOAT3 BoxExtent;

		/** Holds the radius of the bounding sphere. */
		float SphereRadius;

		DirectX::BoundingBox BoxBounds;
		DirectX::BoundingSphere SphereBounds;

		/** Default constructor. */
		BoxSphereBounds() { }

		/**
		 * Creates and initializes a new instance from the specified parameters.
		 *
		 * @param InOrigin origin of the bounding box and sphere.
		 * @param InBoxExtent half size of box.
		 * @param InSphereRadius radius of the sphere.
		 */
		BoxSphereBounds(const XMFLOAT3& InOrigin, const XMFLOAT3& InBoxExtent, float InSphereRadius)
			: Origin(InOrigin)
			, BoxExtent(InBoxExtent)
			, SphereRadius(InSphereRadius)
		{
			BoxBounds = DirectX::BoundingBox(Origin, BoxExtent);
			SphereBounds = DirectX::BoundingSphere(Origin, SphereRadius);
		}
	};	
}

namespace UnrealEngine
{
	using namespace DX;

	using FString = std::wstring;
	using FMatrix = Matrix4;
	using FBoxSphereBounds = BoxSphereBounds;

	template<typename T>
	using TArray = std::vector<T>;

	struct FSceneStaticMeshDataSet
	{
	public:

		FString Name;
		FString OwnerName;
		FString AssetPath;

		uint32 UniqueId;
		uint32 NumVertices;
		uint32 NumTriangles;
		uint32 NumInstances;

		TArray<int32> BoundsIndices;     // First is Mesh...Rest is Instance...
		TArray<int32> TransformsIndices; // First is Mesh...Rest is Instance...
		TArray<int32> UsedMaterialsIndices;
		TArray<int32> UsedMaterialIntancesIndices;

		uint16 NumLODs;
		uint16 CurrentLOD;
	};

	struct FSceneSkeletalMeshDataSet
	{
	public:

		FString Name;
		FString OwnerName;
		FString AssetPath;

		uint32 UniqueId;
		uint32 NumVertices;
		uint32 NumTriangles;
		uint32 NumSections;

		int32 BoundsIndex;
		int32 TransformsIndex;
		TArray<int32> UsedMaterialsIndices;
		TArray<int32> UsedMaterialIntancesIndices;

		uint16 NumLODs;
		uint16 CurrentLOD;
	};

	struct FSceneLandscapeDataSet
	{
	public:

	};

	struct FSceneMaterialDataSet
	{
	public:

		FString Name;
		FString AssetPath;

		// Stats...
		FString TexSamplers;
		FString UserInterpolators;
		FString TexLookups;
		FString VTLookups; // Virtual Texture
		FString ShaderErrors;

		/////////////////////////
		// Material
		FString MaterialDomain;
		FString BlendMode;
		FString DecalBlendMode;
		FString ShadingModel;
		// Translucency...
		FString TranslucencyLightingMode;
		float   TranslucencyDirectionalLightingIntensity;
		// Others...
		uint32 UniqueId;
		uint32 NumInstances;
		uint32 NumRefs;
		TArray<int32> MatInsIndices;
		TArray<int32> UsedTexturesIndices;
		// ShaderInstructionInfo...BPS is Base Pass Shader... 
		int32 BPSCount;
		int32 BPSSurfaceLightmap;
		int32 BPSVolumetricLightmap;
		int32 BPSVertex;
		// Material...
		uint8 TwoSided : 1;
		uint8 bCastRayTracedShadows : 1;
		// Translucency
		uint8 bScreenSpaceReflections : 1;
		uint8 bContactShadows : 1;
		uint8 bUseTranslucencyVertexFog : 1; // Apply Fogging
		uint8 bComputeFogPerPixel : 1;
		uint8 bOutputTranslucentVelocity : 1; // Output Velocity
			// ^ // Advanced...
		uint8 bEnableSeparateTranslucency : 1; // Render After DOF
		uint8 bEnableResponsiveAA : 1;
		uint8 bEnableMobileSeparateTranslucency : 1;
		uint8 bDisableDepthTest : 1;
		uint8 bWriteOnlyAlpha : 1;
		uint8 AllowTranslucentCustomDepthWrites : 1;
		// Mobile
		uint8 bUseFullPrecision : 1;
		uint8 bUseLightmapDirectionality : 1;
		// Forward Shading
		uint8 bUseHQForwardReflections : 1;
		uint8 bUsePlanarForwardReflections : 1;
		/////////////////////////

		bool operator==(const FSceneMaterialDataSet& InElement) const
		{
			if (this->UniqueId == InElement.UniqueId)
				return true;
			else return false;
		}
	};

	struct FSceneMaterialInstanceDataSet
	{
	public:

		FString Name;
		FString AssetPath;

		// ParentData...
		FString ParentName;

		uint32 UniqueId;
		uint32 NumRefs;
		int32  ParentIndex;
		TArray<int32> UsedTexturesIndices;

		bool operator==(const FSceneMaterialInstanceDataSet& InElement) const
		{
			if (this->UniqueId == InElement.UniqueId)
				return true;
			else return false;
		}
	};

	struct FSceneTextureDataSet
	{
	public:

		FString Name;
		FString AssetPath;
		FString Type;

		FString CurrentSize;
		FString PixelFormat;

		FString SourceSize;
		FString SourceFormat;

		uint32 UniqueId;
		uint32 NumRefs;
		int32  LODBias;

		float  CurrentKB;
		float  FullyLoadedKB;

		// Special Format Size...
		float  PVRTC2;
		float  PVRTC4;
		float  ASTC_4x4;	// 8.00 bpp
		float  ASTC_6x6;	// 3.56 bpp
		float  ASTC_8x8;	// 2.00 bpp
		float  ASTC_10x10;	// 1.28 bpp
		float  ASTC_12x12;	// 0.89 bpp

		uint16 CurrentSizeX;
		uint16 CurrentSizeY;
		uint16 SourceSizeX;
		uint16 SourceSizeY;

		uint8  NumResidentMips;
		uint8  NumMipsAllowed;
		uint8  CurrentMips;
		uint8  CompressionNoAlpha : 1;

		bool operator==(const FSceneTextureDataSet& InElement) const
		{
			if (this->UniqueId == InElement.UniqueId)
				return true;
			else return false;
		}
	};

	struct FSceneDataSet
	{
	public:
		TArray<FSceneStaticMeshDataSet>   StaticMeshesTable;
		TArray<FSceneSkeletalMeshDataSet> SkeletalMeshesTable;
		TArray<FSceneLandscapeDataSet>    LandscapesTable;

		TArray<FMatrix> PrimitiveTransforms;

		TArray<FBoxSphereBounds>			  BoundsTable;
		TArray<FSceneMaterialDataSet>		  MaterialsTable;
		TArray<FSceneMaterialInstanceDataSet> MaterialInstancesTable;
		TArray<FSceneTextureDataSet>		  TexturesTable;

		// This is an additional table for lightmap.
		TArray<FSceneTextureDataSet>		  LightMapsAndShadowMaps;
	};
}