#include <catch2/catch_test_macros.hpp>
#include "pattern_matcher.hpp"
#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("PatternMatcher constructor adds default patterns", "[PatternMatcher]") {
    PatternMatcher matcher;
    
    SECTION("Default ignore patterns work") {
        REQUIRE(matcher.isIgnored(".git/config"));
        REQUIRE(matcher.isIgnored("node_modules/package.json"));
        REQUIRE(matcher.isIgnored("build/main.o"));
        REQUIRE(matcher.isIgnored("bin/app.exe"));
        REQUIRE(matcher.isIgnored("lib/libfoo.so"));
        REQUIRE(matcher.isIgnored("__pycache__/module.pyc"));
        REQUIRE(matcher.isIgnored(".DS_Store"));
    }
    
    SECTION("Non-ignored files are not matched") {
        REQUIRE_FALSE(matcher.isIgnored("src/main.cpp"));
        REQUIRE_FALSE(matcher.isIgnored("README.md"));
        REQUIRE_FALSE(matcher.isIgnored("LICENSE"));
        REQUIRE_FALSE(matcher.isIgnored("src/utils/helper.h"));
    }
}

TEST_CASE("PatternMatcher can add custom patterns", "[PatternMatcher]") {
    PatternMatcher matcher;
    
    SECTION("Adding wildcard patterns") {
        matcher.addIgnorePattern("*.txt");
        REQUIRE(matcher.isIgnored("file.txt"));
        REQUIRE(matcher.isIgnored("path/to/file.txt"));
        REQUIRE_FALSE(matcher.isIgnored("file.md"));
    }
    
    SECTION("Adding directory patterns") {
        matcher.addIgnorePattern("build/**");
        REQUIRE(matcher.isIgnored("build/main.cpp"));
        REQUIRE(matcher.isIgnored("build/obj/main.o"));
        REQUIRE_FALSE(matcher.isIgnored("src/build.cpp"));
    }
    
    SECTION("Adding specific file patterns") {
        matcher.addIgnorePattern("src/secret.key");
        REQUIRE(matcher.isIgnored("src/secret.key"));
        REQUIRE_FALSE(matcher.isIgnored("secret.key"));
        REQUIRE_FALSE(matcher.isIgnored("src/not_secret.key"));
    }
}

TEST_CASE("PatternMatcher properly converts patterns to regex", "[PatternMatcher]") {
    PatternMatcher matcher;
    
    SECTION("* wildcard") {
        matcher.addIgnorePattern("*.cpp");
        REQUIRE(matcher.isIgnored("main.cpp"));
        REQUIRE(matcher.isIgnored("helper.cpp"));
        REQUIRE_FALSE(matcher.isIgnored("main.h"));
        REQUIRE_FALSE(matcher.isIgnored("main.cpp/something"));
    }
    
    SECTION("? wildcard") {
        matcher.addIgnorePattern("file?.txt");
        REQUIRE(matcher.isIgnored("file1.txt"));
        REQUIRE(matcher.isIgnored("fileA.txt"));
        REQUIRE_FALSE(matcher.isIgnored("file.txt"));
        REQUIRE_FALSE(matcher.isIgnored("file12.txt"));
    }
    
    SECTION("** wildcard") {
        matcher.addIgnorePattern("src/**/test");
        REQUIRE(matcher.isIgnored("src/test"));
        REQUIRE(matcher.isIgnored("src/foo/test"));
        REQUIRE(matcher.isIgnored("src/foo/bar/test"));
        REQUIRE_FALSE(matcher.isIgnored("foo/test"));
        REQUIRE_FALSE(matcher.isIgnored("src/test/foo"));
    }
}