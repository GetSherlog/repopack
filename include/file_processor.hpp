#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <future>
#include <queue>
#include <memory>
#include "pattern_matcher.hpp"

namespace fs = std::filesystem;

// Forward declaration for SummarizationOptions
struct SummarizationOptions;

// Forward declaration for CodeNER
class CodeNER;

class FileProcessor {
public:
    struct ProcessedFile {
        fs::path path;
        std::string content;
        size_t lineCount;
        size_t byteSize;
        bool isSummarized = false;  // Flag to indicate if the file has been summarized
    };

    // Named Entity for code elements
    struct NamedEntity {
        std::string name;
        enum class EntityType {
            Class,
            Function,
            Variable,
            Enum,
            Import,
            Other
        } type;
        
        // Constructor for easy creation
        NamedEntity(const std::string& n, EntityType t) : name(n), type(t) {}
    };

    explicit FileProcessor(const PatternMatcher& patternMatcher, 
                        unsigned int numThreads = std::thread::hardware_concurrency());
    ~FileProcessor();

    // Process a directory recursively with multithreading
    std::vector<ProcessedFile> processDirectory(const fs::path& dir);

    // Process a single file
    ProcessedFile processFile(const fs::path& filePath);
    
    // Set summarization options
    void setSummarizationOptions(const SummarizationOptions& options);

    // Summarize a file based on the current summarization options
    std::string summarizeFile(const ProcessedFile& file) const;
    
    // Check if a file is a README file
    bool isReadmeFile(const fs::path& filePath) const;

private:
    const PatternMatcher& patternMatcher_;
    unsigned int numThreads_;
    std::vector<std::thread> workers_;
    std::queue<fs::path> fileQueue_;
    std::vector<ProcessedFile> results_;
    std::mutex queueMutex_;
    std::mutex resultsMutex_;
    bool done_;
    SummarizationOptions summarizationOptions_;
    
    // CodeNER instance for entity recognition
    mutable std::unique_ptr<CodeNER> codeNER_;
    
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

    // Summarization helper methods
    std::string extractFirstNLines(const std::string& content, int n) const;
    std::string extractSignatures(const std::string& content, const fs::path& filePath) const;
    std::string extractDocstrings(const std::string& content) const;
    std::string extractRepresentativeSnippets(const std::string& content, int count) const;
    bool shouldSummarizeFile(const ProcessedFile& file) const;
    
    // Get or create the CodeNER instance
    CodeNER* getCodeNER() const;
    
    // Format entities as a string
    std::string formatEntities(const std::vector<NamedEntity>& entities, bool groupByType) const;
};