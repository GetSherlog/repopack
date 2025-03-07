#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <chrono>
#include "file_processor.hpp"
#include "pattern_matcher.hpp"

namespace fs = std::filesystem;

enum class OutputFormat {
    Plain,
    Markdown,
    XML
};

struct RepomixOptions {
    fs::path inputDir;
    fs::path outputFile = "repomix-output.txt";
    OutputFormat format = OutputFormat::Plain;
    bool verbose = false;
    bool showTiming = false;
    unsigned int numThreads = std::thread::hardware_concurrency();
};

class Repomix {
public:
    Repomix(const RepomixOptions& options);
    
    // Run the repomix process
    bool run();
    
    // Get the summary of the processed repository
    std::string getSummary() const;
    
    // Get timing information
    std::string getTimingInfo() const;
    
    // Get the output content directly (useful for WASM)
    std::string getOutput() const;

private:
    RepomixOptions options_;
    std::unique_ptr<FileProcessor> fileProcessor_;
    std::unique_ptr<PatternMatcher> patternMatcher_;
    
    // Output content storage
    std::string outputContent_;
    
    // Statistics
    size_t totalFiles_ = 0;
    size_t totalLines_ = 0;
    size_t totalBytes_ = 0;
    
    // Timing info
    std::chrono::steady_clock::time_point startTime_;
    std::chrono::steady_clock::time_point endTime_;
    std::chrono::milliseconds duration_{0};
    std::chrono::milliseconds processingDuration_{0};
    std::chrono::milliseconds outputDuration_{0};
    
    // Helper methods
    void writeOutput();
    std::string generateDirectoryTree(const fs::path& dir, int level = 0) const;
    std::string formatOutput(const std::vector<FileProcessor::ProcessedFile>& files) const;
};