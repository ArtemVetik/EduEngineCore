#pragma once
#include "Asserts.h"

inline bool StrCmpSuff(const char* str, const char* cmp, const char* suff)
{
	VERIFY_EXPR(str != nullptr, L"str must be not null");
	VERIFY_EXPR(cmp != nullptr, L"cmp must be not null");
	VERIFY_EXPR(suff != nullptr, L"suff must be not null");

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
	VERIFY_EXPR(str != nullptr, L"str must be not null");
	VERIFY_EXPR(suff != nullptr, L"suff must be not null");

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