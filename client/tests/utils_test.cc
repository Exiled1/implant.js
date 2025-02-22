#include <vector>

#include <gtest/gtest.h>

#include "utils.h"

TEST(Utils, ror13) {
    ASSERT_EQ(Utils::ror13("LoadLibraryA"), 0xec0e4e8e);
}

TEST(Utils, get_lines) {
    auto expected = std::vector<std::string>({"hello", "world", "something else"});
    ASSERT_EQ(Utils::get_lines("hello\nworld\nsomething else"), expected);
    ASSERT_EQ(Utils::get_lines("hello|world|something else", '|'), expected);
}

TEST(Utils, merge_lines) {
    auto lines = std::vector<std::string>({"hello", "world", "something else"});
    ASSERT_EQ(Utils::merge_lines(lines), "hello\nworld\nsomething else");
    ASSERT_EQ(Utils::merge_lines(lines, "|"), "hello|world|something else");
}

TEST(Utils, format_string) {
    ASSERT_EQ(Utils::format_string("hello"), "hello");
    ASSERT_EQ(Utils::format_string("hello %s", "world"), "hello world");
}
