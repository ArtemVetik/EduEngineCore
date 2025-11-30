#pragma once
#include "framework.h"

#include "Windows.h"
#include <commdlg.h>

namespace EduEngine
{
	class FileUtils
	{
	public:
		static bool OpenFile(LPCSTR filter, char selectedFile[MAX_PATH])
		{
			char oldDir[MAX_PATH];
			GetCurrentDirectoryA(MAX_PATH, oldDir);

			OPENFILENAMEA ofn;
			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = NULL;
			ofn.lpstrFile = selectedFile;
			ofn.nMaxFile = sizeof(char) * MAX_PATH;
			ofn.lpstrFilter = filter;
			ofn.nFilterIndex = 1;
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

			BOOL result = GetOpenFileNameA(&ofn);

			SetCurrentDirectoryA(oldDir);

			return result;
		}

		static bool OpenFileW(LPCWSTR filter, wchar_t selectedFile[MAX_PATH])
		{
			char oldDir[MAX_PATH];
			GetCurrentDirectoryA(MAX_PATH, oldDir);

			OPENFILENAMEW ofn;
			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = NULL;
			ofn.lpstrFile = selectedFile;
			ofn.nMaxFile = sizeof(wchar_t) * MAX_PATH;
			ofn.lpstrFilter = filter;
			ofn.nFilterIndex = 1;
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

			BOOL result = GetOpenFileNameW(&ofn);

			SetCurrentDirectoryA(oldDir);

			return result;
		}
	};
}