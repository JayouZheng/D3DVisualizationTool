//
// FileManager.h
//

#pragma once

#include <windows.h>
#undef max
#undef min
#include <fstream>
#include <string>
#include <vector>

namespace DX 
{
	namespace FileManager
	{
		template<typename TCHAR = char>
		class FileStream;

		using file_stream = FileStream<char>;
		using wfile_stream = FileStream<wchar_t>;

// 		std::string str0 = DX::to_string(path);
// 		std::string str1 = DX::AppUtil::WStringToString(path);
// 		std::wstring str2 = DX::AppUtil::StringToWString(str1);
// 		std::wstring str3 = DX::to_wstring(str0);
// 
// 		std::locale::global(std::locale(""));
// 		std::wofstream out(L"001.csv");
// 		out << str3 << std::endl;
// 		out.close();
// 		std::ofstream out2(L"002.csv", std::ofstream::binary);
// 		char utf_8_BOM[3] = { int8(0xef),int8(0xbb),int8(0xbf) };
// 		out2.write(utf_8_BOM, 3);
// 		out2.close();
// 		out2.open(L"002.csv", std::ofstream::app);
// 		out2 << str0 << std::endl;
// 		out2.close();

		template<typename TCHAR>
		class FileStream
		{
		public:

			using _file_stream = std::basic_fstream<TCHAR, std::char_traits<TCHAR>>;
			using _string = std::basic_string<TCHAR, std::char_traits<TCHAR>, std::allocator<TCHAR>>;

			FileStream() {}
			~FileStream() { _fstream.close(); }

			FileStream(_string path, std::ios_base::openmode mode = _file_stream::in || _file_stream::out)
			{
				std::locale::global(std::locale(""));
				_fstream.open(path, mode);

				if (_fstream.fail())
				{
				}
			}		

		private:

			_file_stream _fstream;
		};

		class FileUtil
		{
		public:

			// OpenDialog.
			static bool OpenDialogBox(HWND owner, std::wstring& wfile_path, DWORD options);

			static void GetAllFilesUnder(std::string path, std::vector<std::string>& files, std::string format = "");		

			static void WGetAllFilesUnder(std::wstring path, std::vector<std::wstring>& files, std::wstring format = L"");			
			
		};
	}
}
