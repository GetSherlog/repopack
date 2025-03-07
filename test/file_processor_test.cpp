#include <catch2/catch_test_macros.hpp>
#include "file_processor.hpp"
#include "pattern_matcher.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// Helper function to create temporary files for testing
void createTestFile(const fs::path& filePath, const std::string& content) {
    std::ofstream file(filePath);
    file << content;
    file.close();
}

// Helper function to create binary test files
void createBinaryTestFile(const fs::path& filePath, const std::string& content, size_t size) {
    std::ofstream file(filePath, std::ios::binary);
    file.write(content.c_str(), size);
    file.close();
}

TEST_CASE("FileProcessor processes files correctly", "[FileProcessor]") {
    // Create a temporary test directory
    fs::path tempDir = fs::temp_directory_path() / "repomix_test";
    fs::create_directory(tempDir);
    
    // Create some test files
    createTestFile(tempDir / "file1.txt", "Line 1\nLine 2\nLine 3");
    createTestFile(tempDir / "file2.cpp", "int main() {\n    return 0;\n}");
    
    // Create a subdirectory with files
    fs::create_directory(tempDir / "subdir");
    createTestFile(tempDir / "subdir" / "file3.h", "#pragma once\n\nvoid foo();");
    
    // Create an ignored file
    createBinaryTestFile(tempDir / "binary.exe", "Some binary content\0with null bytes", 30);
    
    SECTION("ProcessFile reads file content correctly") {
        PatternMatcher matcher;
        FileProcessor processor(matcher, 1); // Use single thread for testing
        
        auto result = processor.processFile(tempDir / "file1.txt");
        
        REQUIRE(result.path == tempDir / "file1.txt");
        REQUIRE(result.content == "Line 1\nLine 2\nLine 3");
        REQUIRE(result.lineCount == 3);
        REQUIRE(result.byteSize == result.content.size());
    }
    
    SECTION("ProcessDirectory finds all unignored files") {
        PatternMatcher matcher;
        matcher.addIgnorePattern("*.exe");
        FileProcessor processor(matcher, 1); // Use single thread for testing
        
        auto results = processor.processDirectory(tempDir);
        
        // Should find 3 files (file1.txt, file2.cpp, subdir/file3.h)
        // Should ignore binary.exe
        REQUIRE(results.size() == 3);
        
        // Verify each file is found
        bool foundFile1 = false;
        bool foundFile2 = false;
        bool foundFile3 = false;
        
        for (const auto& file : results) {
            if (file.path == tempDir / "file1.txt") {
                foundFile1 = true;
                REQUIRE(file.lineCount == 3);
            } else if (file.path == tempDir / "file2.cpp") {
                foundFile2 = true;
                REQUIRE(file.lineCount == 3);
            } else if (file.path == tempDir / "subdir" / "file3.h") {
                foundFile3 = true;
                REQUIRE(file.lineCount == 3);
            }
        }
        
        REQUIRE(foundFile1);
        REQUIRE(foundFile2);
        REQUIRE(foundFile3);
    }
    
    // Clean up test directory
    fs::remove_all(tempDir);
}

TEST_CASE("FileProcessor counts lines correctly", "[FileProcessor]") {
    PatternMatcher matcher;
    FileProcessor processor(matcher, 1); // Use single thread for testing
    
    // Create a temporary test directory
    fs::path tempDir = fs::temp_directory_path() / "repomix_test";
    fs::create_directory(tempDir);
    
    SECTION("Empty file has 0 lines") {
        const fs::path filePath = tempDir / "empty.txt";
        createTestFile(filePath, "");
        
        auto result = processor.processFile(filePath);
        REQUIRE(result.lineCount == 0);
    }
    
    SECTION("File with one line ending with newline has 1 line") {
        const fs::path filePath = tempDir / "one_line_with_newline.txt";
        createTestFile(filePath, "Line 1\n");
        
        auto result = processor.processFile(filePath);
        REQUIRE(result.lineCount == 1);
    }
    
    SECTION("File with one line not ending with newline has 1 line") {
        const fs::path filePath = tempDir / "one_line_without_newline.txt";
        createTestFile(filePath, "Line 1");
        
        auto result = processor.processFile(filePath);
        REQUIRE(result.lineCount == 1);
    }
    
    SECTION("File with multiple lines counts correctly") {
        const fs::path filePath = tempDir / "multiple_lines.txt";
        createTestFile(filePath, "Line 1\nLine 2\nLine 3\n");
        
        auto result = processor.processFile(filePath);
        REQUIRE(result.lineCount == 3);
    }
    
    // Clean up test directory
    fs::remove_all(tempDir);
}

TEST_CASE("FileProcessor handles non-existent or invalid files", "[FileProcessor]") {
    PatternMatcher matcher;
    FileProcessor processor(matcher, 1); // Use single thread for testing
    
    SECTION("Non-existent file throws exception") {
        REQUIRE_THROWS_AS(processor.processFile("/non/existent/file.txt"), std::runtime_error);
    }
    
    SECTION("Invalid directory throws exception") {
        REQUIRE_THROWS_AS(processor.processDirectory("/non/existent/directory"), std::runtime_error);
    }
}