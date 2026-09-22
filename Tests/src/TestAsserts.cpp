#include "TestAsserts.h"

#include <DxException.h>
#include <gtest/gtest.h>

#include <cassert>
#include <mutex>

namespace EduEngine::Tests
{
	// Assertions may come from any thread, e.g. from concurrent shader compilation
	static std::mutex s_Mutex;
	static std::vector<std::string>* s_CapturedMessages = nullptr;

	static void OnAssert(const std::string& message, const std::string& functionName, const std::string& filename, int lineNumber)
	{
		std::lock_guard<std::mutex> lock(s_Mutex);

		if (s_CapturedMessages != nullptr)
		{
			s_CapturedMessages->push_back(message);
			return;
		}

		ADD_FAILURE_AT(filename.c_str(), lineNumber) << "Engine assertion failed in " << functionName << "(): " << message;
	}

	void InstallAssertHandler()
	{
		DxException::SetAssertHandler(&OnAssert);
	}

	AssertCapture::AssertCapture()
	{
		std::lock_guard<std::mutex> lock(s_Mutex);
		assert(s_CapturedMessages == nullptr && "Nested AssertCapture is not supported");
		s_CapturedMessages = &m_Messages;
	}

	AssertCapture::~AssertCapture()
	{
		std::lock_guard<std::mutex> lock(s_Mutex);
		s_CapturedMessages = nullptr;
	}

	std::vector<std::string> AssertCapture::GetMessages() const
	{
		std::lock_guard<std::mutex> lock(s_Mutex);
		return m_Messages;
	}
}
