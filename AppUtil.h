//
// AppUtil.h
// Header for standard system include files.
//

#pragma once

#include <WinSDKVer.h>
#define _WIN32_WINNT 0x0A00
#include <SDKDDKVer.h>

// Use the C++ standard templated min/max
#define NOMINMAX

// DirectX apps don't need GDI
#define NODRAWTEXT
#define NOGDI
#define NOBITMAP

// Include <mcx.h> if you need this
#define NOMCX

// Include <winsvc.h> if you need this
#define NOSERVICE

// WinHelp is deprecated
#define NOHELP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef max
#undef min
#include <windowsx.h>

#include <wrl/client.h>
#include <wrl/event.h>

#include <d3d12.h>
#include <D3Dcompiler.h>

#if defined(NTDDI_WIN10_RS2)
#include <dxgi1_6.h>
#else
#include <dxgi1_5.h>
#endif

#include <DirectXMath.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>

#include "Common/d3dx12.h"

#include <algorithm>
#include <exception>
#include <memory>
#include <stdexcept>
#include <cassert>
#include <cstdint>

// To use graphics and CPU markup events with the latest version of PIX, change this to include <pix3.h>
// then add the NuGet package WinPixEventRuntime to the project.
#include <pix.h>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

#include "Common/TypeDef.h"
#include "Common/VectorMath.h"

using Microsoft::WRL::ComPtr;

using namespace DirectX;
using namespace Math;

namespace DX
{
	struct Window
	{
		HWND Handle;
		XMINT2 Size;
	};

	class AppUtil
	{
	public:		

		static HWND GetGWindow() { return GWindow; }
		static void SetGWindow(const HWND& window) { GWindow = window; }

		static bool IsKeyDown(int vkeyCode);

		static UINT CalcConstantBufferByteSize(UINT byteSize)
		{
			// Constant buffers must be a multiple of the minimum hardware
			// allocation size (usually 256 bytes).  So round up to nearest
			// multiple of 256.  We do this by adding 255 and then masking off
			// the lower 2 bytes which store all bits < 256.
			// Example: Suppose byteSize = 300.
			// (300 + 255) & ~255
			// 555 & ~255
			// 0x022B & ~0x00ff
			// 0x022B & 0xff00
			// 0x0200
			// 512
			return (byteSize + 255) & ~255;
		}

		static ComPtr<ID3DBlob> LoadBinary(const std::wstring& filename);

		static ComPtr<ID3D12Resource> CreateDefaultBuffer(
			ID3D12Device* device,
			ID3D12GraphicsCommandList* cmdList,
			const void* initData,
			UINT64 byteSize,
			ComPtr<ID3D12Resource>& uploadBuffer);

		static ComPtr<ID3DBlob> CompileShader(
			const std::wstring& filename,
			const D3D_SHADER_MACRO* defines,
			const std::string& entrypoint,
			const std::string& target);		

	private:

		static HWND GWindow;
	};

    // Helper class for COM exceptions
    class com_exception : public std::exception
    {
    public:
        com_exception(HRESULT hr) : result(hr) {}

        virtual const char* what() const override
        {
            static char s_str[64] = {};
            sprintf_s(s_str, "Failure with HRESULT of %08X", static_cast<unsigned int>(result));
            return s_str;
        }

    private:
        HRESULT result;
    };

    // Helper utility converts D3D API failures into exceptions.
    inline void ThrowIfFailedEx(HRESULT hr)
    {
        if (FAILED(hr))
        {
            throw com_exception(hr);
        }
    }

	class DxException
	{
	public:
		DxException() = default;
		DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber);

		std::wstring ToString()const;

		HRESULT ErrorCode = S_OK;
		std::wstring FunctionName;
		std::wstring Filename;
		int LineNumber = -1;
	};

	inline std::wstring AnsiToWString(const std::string& str)
	{
		WCHAR buffer[512];
		MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
		return std::wstring(buffer);
	}

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                            \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
}
#endif

#ifndef ReleaseCom
#define ReleaseCom(x) { if(x){ x->Release(); x = 0; } }
#endif
}