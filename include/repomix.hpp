#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <chrono>
#include "file_processor.hpp"
#include "pattern_matcher.hpp"
#include "tokenizer.hpp"

namespace fs = std::filesystem;

enum class OutputFormat {
    Plain,
    Markdown,
    XML,
    ClaudeXML
};

// Smart file summarization options
struct SummarizationOptions {
    bool enabled = false;                // Enable smart summarization
    bool includeFirstNLines = true;      // Include first N lines of the file
    int firstNLinesCount = 50;           // Number of first lines to include
    bool includeSignatures = true;       // Include function/class signatures
    bool includeDocstrings = true;       // Include docstrings and comments
    bool includeSnippets = false;        // Include representative snippets
    int snippetsCount = 3;               // Number of representative snippets to include
    bool includeReadme = true;           // Include README files
    bool useTreeSitter = true;           // Use Tree-sitter for better summarization
    size_t fileSizeThreshold = 10240;    // Files larger than this (in bytes) will be summarized
    int maxSummaryLines = 200;           // Maximum lines to include in the summary
    
    // Named Entity Recognition options
    bool includeEntityRecognition = false;  // Enable Named Entity Recognition
    
    // NER method selection
    enum class NERMethod {
        Regex,          // Simple regex-based NER (fastest, least accurate)
        TreeSitter,     // Tree-sitter based NER (good balance of speed and accuracy)
        ML,             // Machine Learning based NER (most accurate, slowest)
        Hybrid          // Use Tree-sitter for small files, ML for large files
    };
    NERMethod nerMethod = NERMethod::Regex;  // Default to regex for compatibility
    
    // ML-NER specific options
    bool useMLForLargeFiles = false;       // Use ML for large files (hybrid mode)
    size_t mlNerSizeThreshold = 102400;    // 100KB threshold for ML-NER
    std::string mlModelPath = "";          // Path to ONNX model (empty = use bundled)
    bool cacheMLResults = true;            // Cache ML results to avoid repeat processing
    float mlConfidenceThreshold = 0.7;     // Confidence threshold for ML predictions
    int maxMLProcessingTimeMs = 5000;      // Maximum time to spend on ML processing before fallback
    
    // Entity types to include
    bool includeClassNames = true;       // Include class names in entity list
    bool includeFunctionNames = true;    // Include function/method names in entity list
    bool includeVariableNames = true;    // Include variable names in entity list
    bool includeEnumValues = true;       // Include enum values in entity list
    bool includeImports = true;          // Include imported modules/libraries in entity list
    int maxEntities = 100;               // Maximum number of entities to include
    bool groupEntitiesByType = true;     // Group entities by their type (class, function, etc.)
    
    // Advanced visualization options
    bool includeEntityRelationships = false;  // Include relationships between entities
    bool generateEntityGraph = false;         // Generate a visual graph of entities and relationships
};

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

private:
    RepomixOptions options_;
    std::unique_ptr<FileProcessor> fileProcessor_;
    std::unique_ptr<PatternMatcher> patternMatcher_;
    std::unique_ptr<Tokenizer> tokenizer_;
    
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
    
    // Helper methods
    void writeOutput();
    void countOutputTokens();
    std::string generateDirectoryTree(const fs::path& dir, int level = 0) const;
    std::string formatOutput(const std::vector<FileProcessor::ProcessedFile>& files) const;
};