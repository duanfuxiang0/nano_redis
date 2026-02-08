#include <gtest/gtest.h>
#include "../../../include/core/dashtable.h"

TEST(DashTableDebugTest, BasicInsertion) {
	DashTable<int, int> table(1, 16);

	for (int i = 0; i < 100; ++i) {
		table.Insert(i, i * 10);
	}

	EXPECT_EQ(table.Size(), 100);
	EXPECT_TRUE(table.IsDirectoryConsistent());
}
