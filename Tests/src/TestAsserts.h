#pragma once

#include <string>
#include <vector>

namespace EduEngine::Tests
{
	/// Routes engine assertions (ASSERT_FAILED, VERIFY_EXPR) to gtest instead of a message box:
	/// an assertion fails the current test, unless an AssertCapture is active.
	void InstallAssertHandler();

	/// Collects engine assertions instead of failing the test. For tests that expect them.
	/// Engine assertions exist only in Debug builds, so in Release nothing is collected.
	class AssertCapture
	{
	public:
		AssertCapture();
		~AssertCapture();

		AssertCapture(const AssertCapture&) = delete;
		AssertCapture& operator = (const AssertCapture&) = delete;

		std::vector<std::string> GetMessages() const;

	private:
		std::vector<std::string> m_Messages;
	};
}
