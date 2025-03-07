#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <future>
#include <queue>
#include "pattern_matcher.hpp"

namespace fs = std::filesystem;

class FileProcessor {
public:
    struct ProcessedFile {
        fs::path path;
        std::string content;
        size_t lineCount;
        size_t byteSize;
    };

    explicit FileProcessor(const PatternMatcher& patternMatcher, 
                        unsigned int numThreads = std::thread::hardware_concurrency());
    ~FileProcessor();

    // Process a directory recursively with multithreading
    std::vector<ProcessedFile> processDirectory(const fs::path& dir);

    // Process a single file
    ProcessedFile processFile(const fs::path& filePath);

private:
    const PatternMatcher& patternMatcher_;
    unsigned int numThreads_;
    std::vector<std::thread> workers_;
    std::queue<fs::path> fileQueue_;
    std::vector<ProcessedFile> results_;
    std::mutex queueMutex_;
    std::mutex resultsMutex_;
    bool done_;
    
    // Thread worker function
    void workerThread();
    
    // Collect files to process
    void collectFiles(const fs::path& dir);
    
    // Helper methods
    size_t countLines(const std::string& content) const;
    bool shouldProcessFile(const fs::path& filePath) const;
    
    // Memory mapped file processing
    ProcessedFile processFileWithMemoryMapping(const fs::path& filePath);
    
    // Check if memory mapping is appropriate for the file size
    bool shouldUseMemoryMapping(const fs::path& filePath) const;
};