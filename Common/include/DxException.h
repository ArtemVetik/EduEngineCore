#pragma once
#include <windows.h>
#include <string>
#include <sstream>

namespace EduEngine
{
	inline std::wstring AnsiToWString(const std::string& str)
	{
		WCHAR buffer[512];
		MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
		return std::wstring(buffer);
	}

	typedef std::stringstream MsgStream;

	template<typename SSType, typename ArgType>
	void FormatMsg(SSType& ss, const ArgType& Arg)
	{
		ss << Arg;
	}

	template<typename SSType, typename FirstArgType, typename... RestArgsType>
	void FormatMsg(SSType& ss, const FirstArgType& FirstArg, const RestArgsType&... RestArgs)
	{
		FormatMsg(ss, FirstArg);
		FormatMsg(ss, RestArgs...);
	}

	class DxException
	{
	public:
		static std::wstring GetHRError(HRESULT hr, const std::wstring& message, const std::wstring& functionName, const std::wstring& filename, int lineNumber);
		static void AssertError(const std::string& message, const std::string& functionName, const std::string& filename, int lineNumber);
		static std::string LogError(const std::string& message, const std::string& functionName, const std::string& filename, int lineNumber);
	};
}