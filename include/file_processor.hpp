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
#include <map>
#include <condition_variable>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <functional>
#include "pattern_matcher.hpp"

namespace fs = std::filesystem;

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

// Forward declaration for CodeNER
class CodeNER;

class FileProcessor {
public:
    // Forward declare NamedEntity to use in ProcessedFile
    struct NamedEntity;
    
    struct ProcessedFile {
        fs::path path;
        std::string content;
        size_t lineCount = 0;
        size_t byteSize = 0;
        bool isSummarized = false;      // Flag to indicate if the file has been summarized
        
        // Additional fields for optimized processing
        std::string filename;           // Filename without path
        std::string extension;          // File extension
        std::string error;              // Error message if processing failed
        bool processed = false;         // Flag to indicate successful processing
        bool skipped = false;           // Flag to indicate file was skipped
        
        // Content summary fields
        std::string firstLines;         // First N lines of the file
        std::string snippets;           // Representative snippets
        
        // Entity recognition fields
        std::vector<NamedEntity> entities;  // Named entities found in the file
        std::string formattedEntities;      // Formatted entity text
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
        
        // Default constructor
        NamedEntity() : name(""), type(EntityType::Other) {}
        
        // Constructor for easy creation
        NamedEntity(const std::string& n, EntityType t) : name(n), type(t) {}
    };

    // Progress reporting structure
    struct ProgressInfo {
        size_t totalFiles = 0;           // Total number of files discovered
        size_t processedFiles = 0;        // Number of files processed so far
        size_t skippedFiles = 0;          // Number of files skipped
        size_t errorFiles = 0;            // Number of files with errors
        std::string currentFile = "";     // Path of file currently being processed
        bool isComplete = false;          // Flag indicating if processing is complete
        
        // Default constructor
        ProgressInfo() = default;
        
        // Get progress as percentage (0-100)
        double getPercentage() const {
            if (totalFiles == 0) return 0.0;
            return (static_cast<double>(processedFiles + skippedFiles + errorFiles) / totalFiles) * 100.0;
        }
    };
    
    // Define progress callback type
    using ProgressCallback = std::function<void(const ProgressInfo&)>;

    explicit FileProcessor(const PatternMatcher& patternMatcher, 
                        unsigned int numThreads = std::thread::hardware_concurrency());
    ~FileProcessor();

    /**
     * @brief Process all files in a directory
     * 
     * @param dir Directory to process
     * @param useParallelCollection Whether to use parallel file collection (default: true)
     * @return std::vector<ProcessedFile> Results of processing
     */
    std::vector<ProcessedFile> processDirectory(const fs::path& dir, bool useParallelCollection = true);

    // Process a single file
    ProcessedFile processFile(const fs::path& filePath) const;
    
    // Set summarization options
    void setSummarizationOptions(const SummarizationOptions& options);

    // Summarize a file based on the current summarization options
    std::string summarizeFile(const ProcessedFile& file) const;
    
    // Check if a file is a README file
    bool isReadmeFile(const fs::path& filePath) const;

    /**
     * @brief Process all files in a directory with parallel file collection
     * 
     * @param dir Directory to process
     * @param batchSize Size of batches when adding files to the queue
     * @return std::vector<ProcessedFile> Results of processing
     */
    std::vector<ProcessedFile> processDirectoryParallel(const fs::path& dir, size_t batchSize = 100);

    // Set a callback for progress updates
    void setProgressCallback(ProgressCallback callback);
    
    // Get current progress information
    ProgressInfo getCurrentProgress() const;

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
    
    // Extract named entities from content
    std::vector<NamedEntity> extractNamedEntities(const std::string& content, const fs::path& filePath) const;

    // Maximum file size to process (100 MB)
    static constexpr size_t MAX_FILE_SIZE = 100 * 1024 * 1024;
    
    // File type detection
    bool isBinaryFile(const fs::path& filePath) const;
    
    // Optimized file reading methods
    std::string readFile(const fs::path& filePath) const;
    std::string readLargeFile(const fs::path& filePath, uintmax_t fileSize) const;
    
    // Pre-allocated buffer for file reading to reduce memory allocations
    std::vector<char> fileReadBuffer_;

    // Content flags
    bool keepContent_ = true;
    bool performNER_ = true;

    /**
     * @brief Collects files in parallel from directories
     * 
     * @param dir Root directory to start collection from
     * @param batchSize Size of batches when adding files to the queue
     */
    void collectFilesParallel(const fs::path& dir, size_t batchSize);
    
    /**
     * @brief Worker function for parallel file collection
     * 
     * @param batchSize Size of batches when adding files to the queue
     */
    void fileCollectorWorker(size_t batchSize);
    
    /**
     * @brief Add a directory to the directory queue for parallel processing
     * 
     * @param dir Directory to add
     */
    void addDirectoryToQueue(const fs::path& dir);
    
    // Queue of directories to be processed in parallel
    std::queue<fs::path> directoryQueue_;
    
    // Mutex for directory queue access
    std::mutex dirQueueMutex_;
    
    // Condition variable for directory queue
    std::condition_variable dirQueueCondition_;
    
    // Signal for directory collectors to finish
    std::atomic<bool> dirCollectionDone_{false};

    // Progress tracking members
    ProgressInfo progress_;
    std::mutex progressMutex_;
    ProgressCallback progressCallback_ = nullptr;
    
    // Helper methods for progress tracking
    void updateProgress(size_t totalFiles);
    void incrementProcessedFiles(const fs::path& currentFile = {});
    void incrementSkippedFiles();
    void incrementErrorFiles();
    void setProgressComplete(bool complete = true);
    void reportProgress();
};