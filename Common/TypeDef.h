//
// TypeDef.h
//

#pragma once

#include <cstdint>
#include <cmath>
#include <unordered_map>
#include <map>
#include <array>
#include <vector>
#include <limits>

namespace DX
{
	using int8 = std::int8_t;
	using int16 = std::int16_t;
	using int32 = std::int32_t;
	using int64 = std::int64_t;

	using uint8 = std::uint8_t;
	using uint16 = std::uint16_t;
	using uint32 = std::uint32_t;
	using uint64 = std::uint64_t;

	using byte = uint8;

	typedef void(*PFVOID)();

#define NameOf(x) #x

	enum CameraViewType
	{
		CV_FirstPersonView,
		CV_FocusPointView,
		CV_TopView,
		CV_BottomView,
		CV_LeftView,
		CV_RightView,
		CV_FrontView,
		CV_BackView
	};

	enum CameraProjType
	{
		CP_PerspectiveProj,
		CP_OrthographicProj
	};

	enum FileOpenDialogOptions
	{
		FOS_OVERWRITEPROMPT = 0x2,
		FOS_STRICTFILETYPES = 0x4,
		FOS_NOCHANGEDIR = 0x8,
		FOS_PICKFOLDERS = 0x20,
		FOS_FORCEFILESYSTEM = 0x40,
		FOS_ALLNONSTORAGEITEMS = 0x80,
		FOS_NOVALIDATE = 0x100,
		FOS_ALLOWMULTISELECT = 0x200,
		FOS_PATHMUSTEXIST = 0x800,
		FOS_FILEMUSTEXIST = 0x1000,
		FOS_CREATEPROMPT = 0x2000,
		FOS_SHAREAWARE = 0x4000,
		FOS_NOREADONLYRETURN = 0x8000,
		FOS_NOTESTFILECREATE = 0x10000,
		FOS_HIDEMRUPLACES = 0x20000,
		FOS_HIDEPINNEDPLACES = 0x40000,
		FOS_NODEREFERENCELINKS = 0x100000,
		FOS_OKBUTTONNEEDSINTERACTION = 0x200000,
		FOS_DONTADDTORECENT = 0x2000000,
		FOS_FORCESHOWHIDDEN = 0x10000000,
		FOS_DEFAULTNOMINIMODE = 0x20000000,
		FOS_FORCEPREVIEWPANEON = 0x40000000,
		FOS_SUPPORTSTREAMABLEITEMS = 0x80000000
	};
}
