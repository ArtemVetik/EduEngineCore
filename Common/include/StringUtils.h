#pragma once
#include "Asserts.h"

inline bool StrCmpSuff(const char* str, const char* cmp, const char* suff)
{
	VERIFY_EXPR(str != nullptr, "str must be not null");
	VERIFY_EXPR(cmp != nullptr, "cmp must be not null");
	VERIFY_EXPR(suff != nullptr, "suff must be not null");

	const char* s = &str[0];
	const char* c = &cmp[0];

	while (*c != '\0')
	{
		if (*s != *c)
			return false;

		s++;
		c++;
	}

	return strcmp(s, suff) == 0;
}

inline bool StrHasSuff(const char* str, const char* suff)
{
	VERIFY_EXPR(str != nullptr, "str must be not null");
	VERIFY_EXPR(suff != nullptr, "suff must be not null");

	size_t strLen = strlen(str);
	size_t suffLen = strlen(suff);

	if (suffLen > strLen)
		return false;

	const char* s1 = &str[strLen - suffLen];
	const char* s2 = &suff[0];

	while (suffLen--)
	{
		if (*s1 != *s2)
			return false;

		s1++;
		s2++;
	}

	return true;
}

inline std::wstring ToWString(const std::string& s)
{
	if (s.empty()) return {};

	int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
	std::wstring result(size - 1, 0);

	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, result.data(), size);
	return result;
}

inline std::string WCharToString(const wchar_t* wstr)
{
	int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
	std::string result(size - 1, 0);

	WideCharToMultiByte(CP_UTF8, 0, wstr, -1, result.data(), size, nullptr, nullptr);
	return result;
}
