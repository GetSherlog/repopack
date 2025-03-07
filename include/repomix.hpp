#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <chrono>
#include "file_processor.hpp"
#include "pattern_matcher.hpp"
#include "tokenizer.hpp"
#include "file_scorer.hpp"

namespace fs = std::filesystem;

enum class OutputFormat {
    Plain,
    Markdown,
    XML,
    ClaudeXML
};

// Smart file summarization options defined in file_processor.hpp
// struct SummarizationOptions {...}; 

struct RepomixOptions {
    fs::path inputDir;
    fs::path outputFile = "repomix-output.txt";
    OutputFormat format = OutputFormat::Plain;
    bool verbose = false;
    bool showTiming = false;
    unsigned int numThreads = std::thread::hardware_concurrency();
    std::string includePatterns; // Comma-separated list of glob patterns to include
    std::string excludePatterns; // Comma-separated list of glob patterns to exclude
    SummarizationOptions summarization; // Smart summarization options
    
    // File selection strategy
    enum class FileSelectionStrategy {
        All,            // Include all files (just using ignore patterns)
        Scoring         // Use weighted scoring system to select files
    };
    FileSelectionStrategy selectionStrategy = FileSelectionStrategy::All;
    FileScoringConfig scoringConfig;   // Configuration for file scoring
    
    // Token counting options
    bool countTokens = false;                        // Flag to count tokens in the output
    TokenizerEncoding tokenEncoding = TokenizerEncoding::CL100K_BASE; // Tokenizer to use
    bool onlyShowTokenCount = false;                 // Only display token count without generating the full output
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
    
    // Get token count of the output
    size_t getTokenCount() const;
    
    // Get tokenizer encoding name
    std::string getTokenizerName() const;
    
    // Get file scoring report (if scoring was used)
    std::string getFileScoringReport() const;

private:
    RepomixOptions options_;
    std::unique_ptr<FileProcessor> fileProcessor_;
    std::unique_ptr<PatternMatcher> patternMatcher_;
    std::unique_ptr<Tokenizer> tokenizer_;
    std::unique_ptr<FileScorer> fileScorer_;
    
    // Scored files (if scoring was used)
    std::vector<FileScorer::ScoredFile> scoredFiles_;
    
    // Output content storage
    std::string outputContent_;
    
    // Token count
    size_t tokenCount_ = 0;
    
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
    std::chrono::milliseconds tokenizationDuration_{0};
    std::chrono::milliseconds scoringDuration_{0};
    
    // Helper methods
    void writeOutput();
    void countOutputTokens();
    std::string generateDirectoryTree(const fs::path& dir, int level = 0) const;
    std::string formatOutput(const std::vector<FileProcessor::ProcessedFile>& files) const;
    
    // File selection methods
    std::vector<fs::path> selectFilesUsingScoring(const fs::path& repoPath);
    std::vector<FileProcessor::ProcessedFile> processSelectedFiles(const std::vector<fs::path>& selectedFiles);
};