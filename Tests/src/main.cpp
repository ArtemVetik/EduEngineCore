#include "TestAsserts.h"

#include <gtest/gtest.h>

int main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);

	EduEngine::Tests::InstallAssertHandler();

	return RUN_ALL_TESTS();
}
