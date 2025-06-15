#include "DxException.h"

#include <comdef.h>
#include <signal.h>

namespace EduEngine
{
    std::wstring DxException::GetHRError(HRESULT hr, const std::wstring& message, const std::wstring& functionName, const std::wstring& filename, int lineNumber)
    {
        _com_error err(hr);
        std::wstring msg = err.ErrorMessage();

        return L"DX EXCEPTION: " + functionName + L" failed in " + filename + L"; line " + std::to_wstring(lineNumber) + L"; message: " + message + L". Error: " + msg + L"\n";
    }

    void DxException::AssertError(const std::string& message, const std::string& functionName, const std::string& filename, int lineNumber)
    {
        MsgStream mss;
        FormatMsg(mss, "Debug assertion failed in ", functionName, "(), file ", filename, ", line ", lineNumber, ":\n");

        std::string MBText = mss.str() + "\n" + message;

        int nCode = MessageBoxA(NULL,
            MBText.c_str(),
            "Runtime assertion failed",
            MB_TASKMODAL | MB_ICONHAND | MB_ABORTRETRYIGNORE | MB_SETFOREGROUND);

        if (nCode == IDABORT)
        {
            raise(SIGABRT);

            // We usually won't get here, but it's possible that
            //  SIGABRT was ignored.  So exit the program anyway.
            exit(3);
        }

        if (nCode == IDRETRY)
        {
            DebugBreak();
            return;
        }

        if (nCode == IDIGNORE)
            return;
    }

    std::string DxException::LogError(const std::string & message, const std::string & functionName, const std::string & filename, int lineNumber)
    {
        MsgStream mss;
        FormatMsg(mss, "Error: ", functionName, "(), file ", filename, ", line ", lineNumber, ":\n");

        return mss.str() + "\n" + message;
    }
}