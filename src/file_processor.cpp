#include "file_processor.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <cstring>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <functional>
#include <map>
#include <iostream>
#include "code_ner.hpp"  // Make sure this include is present
#include "repomix.hpp"  // For SummarizationOptions

/**
 * @brief File Processing Flow
 * 
 * ```mermaid
 * flowchart TD
 *     Start([Start]) --> ProcessDir[Process Directory]
 *     ProcessDir --> CollectFiles[Collect Files]
 *     CollectFiles --> ThreadPool{Create Thread Pool}
 *     
 *     ThreadPool -->|Success| ParallelProc[Parallel Processing]
 *     ThreadPool -->|Failure| SequentialProc[Sequential Processing]
 *     
 *     ParallelProc --> WorkerThread[Worker Thread]
 *     WorkerThread --> GetFile[Get File from Queue]
 *     
 *     GetFile -->|Empty| End([End])
 *     GetFile -->|File Available| ProcessFile[Process File]
 *     
 *     ProcessFile --> ValidateFile{Validate File}
 *     ValidateFile -->|Invalid| LogError[Log Error]
 *     ValidateFile -->|Valid| CheckSize{Check Size}
 *     
 *     CheckSize -->|Large| MemoryMap[Use Memory Mapping]
 *     CheckSize -->|Small| BufferedRead[Use Buffered Reading]
 *     
 *     MemoryMap --> Extract[Extract Information]
 *     BufferedRead --> Extract
 *     
 *     Extract --> NER[Named Entity Recognition]
 *     Extract --> Lines[Count Lines]
 *     Extract --> Snippets[Extract Snippets]
 *     Extract --> Signatures[Extract Signatures]
 *     Extract --> Comments[Extract Comments]
 *     
 *     NER --> Summarize{Should Summarize?}
 *     Lines --> Summarize
 *     Snippets --> Summarize
 *     Signatures --> Summarize
 *     Comments --> Summarize
 *     
 *     Summarize -->|Yes| CreateSummary[Create Summary]
 *     Summarize -->|No| StoreResult[Store Result]
 *     CreateSummary --> StoreResult
 *     
 *     StoreResult --> GetFile
 *     LogError --> GetFile
 *     
 *     SequentialProc --> ProcessFile
 * ```
 */

// Size threshold for using memory mapping (files larger than this will use memory mapping)
constexpr size_t MMAP_THRESHOLD = 1 * 1024 * 1024; // 1 MB

// Add a buffer size for reading files
constexpr size_t FILE_BUFFER_SIZE = 128 * 1024; // 128 KB

/**
 * @brief Constructs a new FileProcessor object
 * 
 * @param patternMatcher Reference to a PatternMatcher object used to determine which files to process
 * @param numThreads Number of threads to use for parallel processing. If 0, defaults to 1 thread
 * 
 * Initializes a FileProcessor with the specified pattern matcher and number of threads.
 * Pre-allocates buffers that will be reused during file processing.
 */
FileProcessor::FileProcessor(const PatternMatcher& patternMatcher, unsigned int numThreads)
    : patternMatcher_(patternMatcher), 
      numThreads_(numThreads == 0 ? 1 : numThreads),
      done_(false) {
    // Pre-allocate buffers that will be reused
    fileReadBuffer_.resize(FILE_BUFFER_SIZE);
}

/**
 * @brief Destroys the FileProcessor object
 * 
 * Ensures all worker threads are properly joined before destruction.
 * Sets the done_ flag to true to signal threads to stop processing.
 */
FileProcessor::~FileProcessor() {
    // Make sure threads are joined
    done_ = true;
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

/**
 * @brief Processes all files in a directory using multiple threads
 * 
 * @param dir Path to the directory to process
 * @param useParallelCollection Whether to use parallel file collection
 * @return std::vector<FileProcessor::ProcessedFile> Vector of processed file results
 * @throws std::runtime_error if the directory is invalid or cannot be accessed
 * 
 * This method:
 * 1. Validates the input directory
 * 2. Collects all files that should be processed
 * 3. Creates worker threads to process files in parallel
 * 4. Falls back to sequential processing if thread creation fails
 * 5. Returns results of all processed files
 */
std::vector<FileProcessor::ProcessedFile> FileProcessor::processDirectory(const fs::path& dir, bool useParallelCollection) {
    // Validate directory
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        throw std::runtime_error("Invalid directory: " + dir.string());
    }
    
    // If using parallel collection, delegate to that implementation
    if (useParallelCollection) {
        // Use a reasonable default batch size
        const size_t defaultBatchSize = 100;
        return processDirectoryParallel(dir, defaultBatchSize);
    }
    
    // Otherwise, use the original implementation
    // Reset state
    done_ = false;
    fileQueue_ = std::queue<fs::path>();
    results_.clear();
    
    collectFiles(dir);
    
    // If we have files to process, start worker threads
    if (!fileQueue_.empty()) {
        // Create worker threads (use at most numThreads_ or fileQueue_.size() threads)
        unsigned int actualThreads = std::min(numThreads_, static_cast<unsigned int>(fileQueue_.size()));
        workers_.clear();
        
        bool threadsStarted = true;
        
        try {
            // Create worker threads
            for (unsigned int i = 0; i < actualThreads; ++i) {
                try {
                    workers_.emplace_back(&FileProcessor::workerThread, this);
                } catch (const std::system_error& e) {
                    // If we can't create more threads, just use what we have
                    std::cerr << "Warning: Could not create additional thread: " << e.what() << std::endl;
                    break;
                }
            }
            
            // Wait for all worker threads to finish
            for (auto& worker : workers_) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
        } catch (const std::system_error& e) {
            threadsStarted = false;
            
            // Clean up any threads that were created
            done_ = true;
            for (auto& worker : workers_) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
            workers_.clear();
        }
        
        // If thread creation failed, process files sequentially
        if (!threadsStarted) {
            while (!fileQueue_.empty()) {
                fs::path filePath = fileQueue_.front();
                fileQueue_.pop();
                
                try {
                    ProcessedFile processedFile;
                    
                    // Check if we should use memory mapping for this file
                    if (shouldUseMemoryMapping(filePath)) {
                        processedFile = processFileWithMemoryMapping(filePath);
                    } else {
                        processedFile = processFile(filePath);
                    }
                    
                    // Add result to the result vector
                    results_.push_back(std::move(processedFile));
                }
                catch (const std::exception& e) {
                    std::cerr << "Warning: Failed to process file " << filePath << ": " << e.what() << std::endl;
                }
            }
        }
    }
    
    return results_;
}

/**
 * @brief Collects all files from a directory that should be processed
 * 
 * @param dir Path to the directory to scan
 * 
 * Recursively traverses the directory and adds files to the processing queue
 * if they match the criteria defined by the pattern matcher.
 * Uses a two-pass approach to minimize lock contention:
 * 1. First collects files without locking
 * 2. Then adds all files to the queue in a single lock operation
 */
void FileProcessor::collectFiles(const fs::path& dir) {
    try {
        std::vector<fs::path> files;
        files.reserve(1000);
        
        // First pass: collect all files without locking the mutex
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (fs::is_regular_file(entry.path())) {
                // Check if file should be processed
                if (shouldProcessFile(entry.path())) {
                    files.push_back(entry.path());
                }
            }
        }
        
        // Second pass: add all files to queue in one lock operation
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            for (const auto& file : files) {
                fileQueue_.push(file);
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error collecting files: " << e.what() << std::endl;
    }
}

/**
 * @brief Worker thread function that processes files from the queue
 * 
 * Continuously retrieves files from the shared queue and processes them
 * until either the queue is empty or the done_ flag is set.
 * Results are added to the shared results vector in a thread-safe manner.
 * Any errors during processing are caught and logged, with error entries
 * added to the results.
 */
void FileProcessor::workerThread() {
    while (!done_) {
        fs::path filePath;
        
        // Try to get a file from the queue
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            if (fileQueue_.empty()) {
                // No more files, we're done
                return;
            }
            
            filePath = fileQueue_.front();
            fileQueue_.pop();
        }
        
        // Process the file outside the lock
        try {
            ProcessedFile result = processFile(filePath);
            
            // Only add to results if successfully processed
            if (result.processed || result.skipped) {
                std::lock_guard<std::mutex> lock(resultsMutex_);
                results_.push_back(std::move(result));
            }
        } catch (const std::exception& e) {
            // Log error and continue with next file
            std::cerr << "Error processing file " << filePath << ": " << e.what() << std::endl;
            
            // Add error entry to results
            ProcessedFile errorResult;
            errorResult.path = filePath;
            errorResult.filename = filePath.filename().string();
            errorResult.error = e.what();
            
            std::lock_guard<std::mutex> lock(resultsMutex_);
            results_.push_back(std::move(errorResult));
        }
    }
}

/**
 * @brief Processes a single file and returns the results
 * 
 * @param filePath Path to the file to process
 * @return ProcessedFile Structure containing the processing results
 * 
 * This method:
 * 1. Validates the file exists and is accessible
 * 2. Checks file size and type constraints
 * 3. Reads and processes the file content
 * 4. Extracts metadata like line count and snippets
 * 5. Performs named entity recognition if enabled
 * 
 * The method handles various error conditions and sets appropriate
 * error messages in the result structure if processing fails.
 */
FileProcessor::ProcessedFile FileProcessor::processFile(const fs::path& filePath) const {
    ProcessedFile result;
    result.path = filePath;
    result.filename = filePath.filename().string();
    result.extension = filePath.extension().string();
    
    if (!fs::exists(filePath) || !fs::is_regular_file(filePath)) {
        result.error = "File does not exist or is not a regular file";
        return result;
    }
    
    // Skip if file exceeds size limit
    std::error_code ec;
    auto fileSize = fs::file_size(filePath, ec);
    if (ec) {
        result.error = "Error getting file size: " + ec.message();
        return result;
    }
    
    if (fileSize > MAX_FILE_SIZE) {
        result.error = "File too large, skipping";
        result.skipped = true;
        return result;
    }
    
    // Skip binary files
    if (isBinaryFile(filePath)) {
        result.error = "Binary file detected, skipping";
        result.skipped = true;
        return result;
    }
    
    try {
        // Use our optimized file reading function
        std::string content = readFile(filePath);
        
        // Store content if keeping content
        if (keepContent_) {
            result.content = content;
        }
        
        // Extract first N lines as a summary
        result.firstLines = extractFirstNLines(content, 50);
        
        // Extract representative snippets
        result.snippets = extractRepresentativeSnippets(content, 3);
        
        // Get line count
        result.lineCount = countLines(content);
        
        // Perform named entity recognition if requested
        if (performNER_) {
            result.entities = extractNamedEntities(content, filePath);
            result.formattedEntities = formatEntities(result.entities, true);
        }
        
        // Mark as processed successfully
        result.processed = true;
    } catch (const std::exception& e) {
        result.error = std::string("Error processing file: ") + e.what();
    }
    
    return result;
}

/**
 * @brief Determines if a file should be processed using memory mapping
 * 
 * @param filePath Path to the file to check
 * @return bool True if the file should use memory mapping, false otherwise
 * 
 * Memory mapping is used for files larger than MMAP_THRESHOLD (1MB).
 * The method checks if the file exists and is a regular file before
 * making the determination.
 */
bool FileProcessor::shouldUseMemoryMapping(const fs::path& filePath) const {
    // Check if file exists and is larger than threshold
    try {
        return fs::exists(filePath) && fs::is_regular_file(filePath) && 
               fs::file_size(filePath) > MMAP_THRESHOLD;
    }
    catch (...) {
        return false;
    }
}

/**
 * @brief Processes a file using memory mapping for efficient reading
 * 
 * @param filePath Path to the file to process
 * @return ProcessedFile Structure containing the processing results
 * @throws std::runtime_error if memory mapping fails
 * 
 * Uses memory mapping for efficient processing of large files.
 * Falls back to regular file processing if memory mapping fails.
 * Handles empty files and properly unmaps memory when done.
 */
FileProcessor::ProcessedFile FileProcessor::processFileWithMemoryMapping(const fs::path& filePath) {
    ProcessedFile result;
    result.path = filePath;
    
    int fd = open(filePath.c_str(), O_RDONLY);
    if (fd == -1) {
        throw std::runtime_error("Failed to open file for memory mapping: " + filePath.string());
    }
    
    // Get file size
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        close(fd);
        throw std::runtime_error("Failed to get file size for memory mapping: " + filePath.string());
    }
    
    // Map file into memory
    const size_t fileSize = static_cast<size_t>(sb.st_size);
    
    if (fileSize == 0) {
        // Empty file, just return empty content
        close(fd);
        result.content = "";
        result.lineCount = 0;
        result.byteSize = 0;
        return result;
    }
    
    void* mapped = mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
    
    if (mapped == MAP_FAILED) {
        close(fd);
        // Fallback to regular file processing if memory mapping fails
        return processFile(filePath);
    }
    
    // Copy from mapped memory to string (we still need a copy because we need to modify it for line counting)
    result.content.assign(static_cast<char*>(mapped), fileSize);
    result.byteSize = fileSize;
    
    // Count lines
    result.lineCount = countLines(result.content);
    
    // Unmap and close
    munmap(mapped, fileSize);
    close(fd);
    
    return result;
}

/**
 * @brief Counts the number of lines in a string
 * 
 * @param content The string content to count lines in
 * @return size_t The number of lines in the content
 * 
 * Uses an optimized approach with std::count to count newlines.
 * Handles the special case where the last line doesn't end with a newline.
 */
size_t FileProcessor::countLines(const std::string& content) const {
    // Optimized line counting using std::count
    size_t count = std::count(content.begin(), content.end(), '\n');
    
    // If the last line doesn't end with a newline, count it too
    if (!content.empty() && content.back() != '\n') {
        ++count;
    }
    
    return count;
}

/**
 * @brief Determines if a file should be processed based on various criteria
 * 
 * @param filePath Path to the file to check
 * @return bool True if the file should be processed, false otherwise
 * 
 * Checks multiple criteria:
 * 1. Must be a regular file
 * 2. Size must be under MAX_FILE_SIZE (100MB)
 * 3. Must not be a binary file
 * 4. Must match patterns defined in patternMatcher_
 * 
 * Uses a simple heuristic to detect binary files by checking for null bytes
 * in the first few KB of the file.
 */
bool FileProcessor::shouldProcessFile(const fs::path& filePath) const {
    // Skip if not a regular file
    if (!fs::is_regular_file(filePath)) {
        return false;
    }
    
    // Skip files larger than a reasonable size (100 MB)
    constexpr size_t MAX_FILE_SIZE = 100 * 1024 * 1024;
    try {
        if (fs::file_size(filePath) > MAX_FILE_SIZE) {
            return false;
        }
    }
    catch (...) {
        return false;
    }
    
    // Avoid processing binary files (a simple heuristic)
    try {
        // Read the first few bytes to check for binary content
        const size_t bytesToCheck = 8192;
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            return false;
        }
        
        char buffer[bytesToCheck];
        const auto bytesRead = file.read(buffer, bytesToCheck).gcount();
        
        // Check for null bytes (common in binary files)
        for (int i = 0; i < bytesRead; ++i) {
            if (buffer[i] == '\0') {
                return false;
            }
        }
    }
    catch (...) {
        return false;
    }
    
    // Use the pattern matcher to determine if file should be processed
    return patternMatcher_.shouldProcess(filePath);
}

/**
 * @brief Sets the options for file summarization
 * 
 * @param options The summarization options to use
 * 
 * Updates the internal summarization options that control how files
 * are summarized during processing. These options affect what information
 * is extracted and included in the file summaries.
 */
void FileProcessor::setSummarizationOptions(const SummarizationOptions& options) {
    summarizationOptions_ = options;
}

/**
 * @brief Determines if a file should be summarized based on size and settings
 * 
 * @param file The processed file to check
 * @return bool True if the file should be summarized, false otherwise
 * 
 * Checks if summarization is enabled and if the file size exceeds
 * the configured threshold in the summarization options.
 */
bool FileProcessor::shouldSummarizeFile(const ProcessedFile& file) const {
    if (!summarizationOptions_.enabled) {
        return false;
    }
    
    return file.byteSize > summarizationOptions_.fileSizeThreshold;
}

/**
 * @brief Creates a summary of a processed file
 * 
 * @param file The processed file to summarize
 * @return std::string The generated summary
 * 
 * Generates a comprehensive summary of the file based on enabled options:
 * - Named Entity Recognition results
 * - First N lines
 * - Function and class signatures
 * - Docstrings and comments
 * - Representative code snippets
 * 
 * Special handling is applied for README files if the option is enabled.
 * The summary includes headers indicating which summarization techniques
 * were applied.
 */
std::string FileProcessor::summarizeFile(const ProcessedFile& file) const {
    if (!shouldSummarizeFile(file)) {
        return file.content; // Return original content if summarization not needed
    }
    
    std::stringstream summary;
    
    // Add a header indicating this is a summary
    summary << "/* SUMMARIZED FILE: " << file.path.filename().string() << " */" << std::endl;
    summary << "/* Original size: " << file.byteSize << " bytes, " << file.lineCount << " lines */" << std::endl;
    summary << "/* The file has been summarized using the following techniques: */" << std::endl;
    
    bool atLeastOneTechnique = false;
    
    // Include Named Entity Recognition
    if (summarizationOptions_.includeEntityRecognition) {
        summary << "/* - Named Entity Recognition";
        
        // Add method information
        switch (summarizationOptions_.nerMethod) {
            case SummarizationOptions::NERMethod::Regex:
                summary << " (Regex)";
                break;
            case SummarizationOptions::NERMethod::TreeSitter:
                summary << " (Tree-sitter)";
                break;
            case SummarizationOptions::NERMethod::ML:
                summary << " (Machine Learning)";
                break;
            case SummarizationOptions::NERMethod::Hybrid:
                summary << " (Hybrid)";
                break;
        }
        
        summary << " */" << std::endl;
        atLeastOneTechnique = true;
    }
    
    // Include first N lines
    if (summarizationOptions_.includeFirstNLines) {
        summary << "/* - First " << summarizationOptions_.firstNLinesCount << " lines */" << std::endl;
        atLeastOneTechnique = true;
    }
    
    // Include signatures
    if (summarizationOptions_.includeSignatures) {
        summary << "/* - Function and class signatures */" << std::endl;
        atLeastOneTechnique = true;
    }
    
    // Include docstrings
    if (summarizationOptions_.includeDocstrings) {
        summary << "/* - Docstrings and comments */" << std::endl;
        atLeastOneTechnique = true;
    }
    
    // Include representative snippets
    if (summarizationOptions_.includeSnippets) {
        summary << "/* - " << summarizationOptions_.snippetsCount << " representative code snippets */" << std::endl;
        atLeastOneTechnique = true;
    }
    
    // Include README special handling
    if (summarizationOptions_.includeReadme && isReadmeFile(file.path)) {
        summary << "/* - Full README content */" << std::endl;
        atLeastOneTechnique = true;
        return file.content; // Always include full README files if this option is enabled
    }
    
    summary << "/* */" << std::endl << std::endl;
    
    // If no summarization technique was applied, return original content
    if (!atLeastOneTechnique) {
        return file.content;
    }
    
    std::vector<std::string> summaryLines;
    
    // Extract and add named entities first if enabled
    if (summarizationOptions_.includeEntityRecognition) {
        CodeNER* nerSystem = getCodeNER();
        if (nerSystem) {
            auto entities = nerSystem->extractEntities(file.content, file.path.string());
            std::string entitySummary = formatEntities(entities, summarizationOptions_.groupEntitiesByType);
            
            if (!entitySummary.empty()) {
                std::string nerMethod;
                switch (summarizationOptions_.nerMethod) {
                    case SummarizationOptions::NERMethod::Regex:
                        nerMethod = "REGEX";
                        break;
                    case SummarizationOptions::NERMethod::TreeSitter:
                        nerMethod = "TREE-SITTER";
                        break;
                    case SummarizationOptions::NERMethod::ML:
                        nerMethod = "ML";
                        break;
                    case SummarizationOptions::NERMethod::Hybrid:
                        nerMethod = "HYBRID";
                        break;
                }
                summary << "/* --- NAMED ENTITY RECOGNITION SUMMARY (" << nerMethod << ") --- */" << std::endl;
                summary << entitySummary;
            }
        }
    }
    
    // Add first N lines
    if (summarizationOptions_.includeFirstNLines) {
        std::string firstLines = extractFirstNLines(file.content, summarizationOptions_.firstNLinesCount);
        if (!firstLines.empty()) {
            summary << "/* --- FIRST " << summarizationOptions_.firstNLinesCount << " LINES --- */" << std::endl;
            summary << firstLines << std::endl << std::endl;
        }
    }
    
    // Add function/class signatures
    if (summarizationOptions_.includeSignatures) {
        std::string signatures = extractSignatures(file.content, file.path);
        if (!signatures.empty()) {
            summary << "/* --- FUNCTION & CLASS SIGNATURES --- */" << std::endl;
            summary << signatures << std::endl << std::endl;
        }
    }
    
    // Add docstrings and comments
    if (summarizationOptions_.includeDocstrings) {
        std::string docstrings = extractDocstrings(file.content);
        if (!docstrings.empty()) {
            summary << "/* --- DOCSTRINGS & COMMENTS --- */" << std::endl;
            summary << docstrings << std::endl << std::endl;
        }
    }
    
    // Add representative snippets
    if (summarizationOptions_.includeSnippets) {
        std::string snippets = extractRepresentativeSnippets(file.content, summarizationOptions_.snippetsCount);
        if (!snippets.empty()) {
            summary << "/* --- REPRESENTATIVE SNIPPETS --- */" << std::endl;
            summary << snippets << std::endl << std::endl;
        }
    }
    
    return summary.str();
}

/**
 * @brief Extracts the first N lines from a string
 * 
 * @param content The string content to extract lines from
 * @param n The number of lines to extract
 * @return std::string The first N lines of content
 * 
 * Uses a stream-based approach to efficiently extract the first N lines
 * from the content. Preserves line endings in the extracted text.
 */
std::string FileProcessor::extractFirstNLines(const std::string& content, int n) const {
    std::istringstream iss(content);
    std::string line;
    std::stringstream result;
    int count = 0;
    
    while (std::getline(iss, line) && count < n) {
        result << line << std::endl;
        count++;
    }
    
    return result.str();
}

/**
 * @brief Extracts function and class signatures from code
 * 
 * @param content The source code content
 * @param filePath The path to the source file (used to determine language)
 * @return std::string Extracted signatures formatted as a string
 * 
 * Supports multiple programming languages:
 * - C/C++: Functions, classes, and structs
 * - Python: Functions and classes
 * - JavaScript/TypeScript: Functions, classes, and methods
 * 
 * Uses regex patterns tailored to each language to extract signatures.
 * Handles various function/method declaration styles and modifiers.
 */
std::string FileProcessor::extractSignatures(const std::string& content, const fs::path& filePath) const {
    std::stringstream result;
    std::string extension = filePath.extension().string();
    
    // Simple regex-based extraction of signatures
    if (extension == ".cpp" || extension == ".hpp" || extension == ".h" || extension == ".c") {
        // C/C++ function signatures
        std::regex functionRegex(R"((\w+\s+)*\w+\s+\w+\s*\([^{;]*\)\s*(?:const)?\s*(?:noexcept)?\s*(?:override)?\s*(?:final)?\s*(?:=\s*0)?\s*(?:=\s*delete)?\s*(?:=\s*default)?\s*(?:;|{))");
        std::regex classRegex(R"((class|struct)\s+\w+\s*(?::\s*(?:public|protected|private)\s+\w+(?:::\w+)?(?:\s*,\s*(?:public|protected|private)\s+\w+(?:::\w+)?)*\s*)?\s*\{)");
        
        std::sregex_iterator funcIt(content.begin(), content.end(), functionRegex);
        std::sregex_iterator classIt(content.begin(), content.end(), classRegex);
        std::sregex_iterator end;
        
        // Extract and add function signatures
        for (; funcIt != end; ++funcIt) {
            std::string match = funcIt->str();
            // Remove function body if present (keep only the signature)
            size_t bracePos = match.find('{');
            if (bracePos != std::string::npos) {
                match = match.substr(0, bracePos + 1) + "...}";
            }
            result << match << std::endl;
        }
        
        // Extract and add class signatures
        for (; classIt != end; ++classIt) {
            std::string match = classIt->str();
            result << match << "...};" << std::endl;
        }
    } 
    else if (extension == ".py") {
        // Python function and class signatures
        std::regex functionRegex(R"(def\s+\w+\s*\([^:]*\)\s*(?:->.*?)?\s*:)");
        std::regex classRegex(R"(class\s+\w+(?:\([^:]*\))?\s*:)");
        
        std::sregex_iterator funcIt(content.begin(), content.end(), functionRegex);
        std::sregex_iterator classIt(content.begin(), content.end(), classRegex);
        std::sregex_iterator end;
        
        for (; funcIt != end; ++funcIt) {
            result << funcIt->str() << std::endl;
        }
        
        for (; classIt != end; ++classIt) {
            result << classIt->str() << std::endl;
        }
    }
    else if (extension == ".js" || extension == ".ts" || extension == ".jsx" || extension == ".tsx") {
        // JavaScript/TypeScript function and class signatures
        std::regex functionRegex(R"((async\s+)?function\s+\w+\s*\([^{]*\)|const\s+\w+\s*=\s*(async\s+)?\([^{]*\)\s*=>)");
        std::regex classRegex(R"(class\s+\w+(?:\s+extends\s+\w+)?\s*\{)");
        std::regex methodRegex(R"((\w+)\s*\([^{]*\)\s*\{)");
        
        std::sregex_iterator funcIt(content.begin(), content.end(), functionRegex);
        std::sregex_iterator classIt(content.begin(), content.end(), classRegex);
        std::sregex_iterator methodIt(content.begin(), content.end(), methodRegex);
        std::sregex_iterator end;
        
        for (; funcIt != end; ++funcIt) {
            result << funcIt->str() << " {...}" << std::endl;
        }
        
        for (; classIt != end; ++classIt) {
            result << classIt->str() << "...}" << std::endl;
        }
        
        for (; methodIt != end; ++methodIt) {
            result << methodIt->str() << "...}" << std::endl;
        }
    }
    
    return result.str();
}

/**
 * @brief Extracts docstrings and important comments from code
 * 
 * @param content The source code content
 * @return std::string Extracted documentation formatted as a string
 * 
 * Extracts multiple types of documentation:
 * - Multi-line comments 
 * - Single-line comments (// style)
 * - Python docstrings ("""...""" or '''...''')
 * 
 * Preserves the original formatting of the extracted documentation.
 */
std::string FileProcessor::extractDocstrings(const std::string& content) const {
    std::stringstream result;
    std::regex multiLineCommentRegex(R"(/\*[\s\S]*?\*/)");
    std::regex singleLineCommentRegex(R"(//.*$)");
    std::regex pythonDocstringRegex(R"("""[\s\S]*?"""|'''[\s\S]*?''')");
    
    // Extract multi-line comments (C-style)
    std::sregex_iterator multiIt(content.begin(), content.end(), multiLineCommentRegex);
    std::sregex_iterator end;
    
    for (; multiIt != end; ++multiIt) {
        result << multiIt->str() << std::endl;
    }
    
    // Extract single-line comments
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        std::smatch match;
        if (std::regex_search(line, match, singleLineCommentRegex)) {
            result << match.str() << std::endl;
        }
    }
    
    // Extract Python docstrings
    std::sregex_iterator docIt(content.begin(), content.end(), pythonDocstringRegex);
    
    for (; docIt != end; ++docIt) {
        result << docIt->str() << std::endl;
    }
    
    return result.str();
}

/**
 * @brief Extracts representative code snippets from different parts of the file
 * 
 * @param content The source code content
 * @param count Number of snippets to extract
 * @return std::string Extracted snippets formatted as a string
 * 
 * Extracts evenly spaced snippets from throughout the file to provide
 * a representative sample of the code. Each snippet includes line numbers
 * and is properly formatted with headers.
 */
std::string FileProcessor::extractRepresentativeSnippets(const std::string& content, int count) const {
    std::stringstream result;
    std::istringstream iss(content);
    std::vector<std::string> lines;
    std::string line;
    
    // Collect all lines
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }
    
    if (lines.empty()) {
        return "";
    }
    
    // Determine snippet size and locations
    size_t totalLines = lines.size();
    size_t snippetSize = std::min(size_t(20), totalLines / count);
    
    // If the file is too small for meaningful snippets, return empty
    if (snippetSize < 5) {
        return "";
    }
    
    // Extract 'count' snippets at approximately evenly spaced intervals
    for (int i = 0; i < count; i++) {
        size_t startLine = (i * totalLines) / count;
        
        result << "/* Snippet " << (i + 1) << " (lines " << (startLine + 1) 
               << "-" << (startLine + snippetSize) << ") */" << std::endl;
        
        for (size_t j = 0; j < snippetSize && (startLine + j) < totalLines; j++) {
            result << lines[startLine + j] << std::endl;
        }
        
        result << std::endl;
    }
    
    return result.str();
}

/**
 * @brief Checks if a file is a README file
 * 
 * @param filePath Path to the file to check
 * @return bool True if the file is a README file, false otherwise
 * 
 * Checks various common README file patterns:
 * - readme.md
 * - readme.txt
 * - readme (no extension)
 * - Any file starting with readme.
 * Case-insensitive matching is used.
 */
bool FileProcessor::isReadmeFile(const fs::path& filePath) const {
    std::string filename = filePath.filename().string();
    std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
    
    return (filename == "readme.md" || filename == "readme.txt" || 
            filename == "readme" || filename.find("readme.") == 0);
}

/**
 * @brief Gets the CodeNER instance for named entity recognition
 * 
 * @return CodeNER* Pointer to the CodeNER instance, or nullptr if not available
 * 
 * Lazily initializes the CodeNER instance when first needed.
 * Only creates the instance if entity recognition is enabled in the options.
 */
CodeNER* FileProcessor::getCodeNER() const {
    // Lazy initialization of the CodeNER instance
    if (!codeNER_ && summarizationOptions_.includeEntityRecognition) {
        codeNER_ = CodeNER::create(summarizationOptions_);
    }
    return codeNER_.get();
}

/**
 * @brief Formats extracted named entities into a readable string
 * 
 * @param entities Vector of named entities to format
 * @param groupByType Whether to group entities by their type
 * @return std::string Formatted string representation of the entities
 * 
 * If groupByType is true, entities are grouped into categories:
 * - Classes
 * - Functions
 * - Variables
 * - Enums
 * - Imports/Includes
 * - Other
 * 
 * Each category is properly formatted with headers and consistent spacing.
 */
std::string FileProcessor::formatEntities(const std::vector<NamedEntity>& entities, bool groupByType) const {
    if (entities.empty()) {
        return "";
    }
    
    std::stringstream result;
    
    if (groupByType) {
        // Group entities by type
        std::map<NamedEntity::EntityType, std::vector<std::string>> groupedEntities;
        
        for (const auto& entity : entities) {
            groupedEntities[entity.type].push_back(entity.name);
        }
        
        // Output classes
        if (!groupedEntities[NamedEntity::EntityType::Class].empty()) {
            result << "/* --- CLASSES --- */" << std::endl;
            for (const auto& name : groupedEntities[NamedEntity::EntityType::Class]) {
                result << name << std::endl;
            }
            result << std::endl;
        }
        
        // Output functions
        if (!groupedEntities[NamedEntity::EntityType::Function].empty()) {
            result << "/* --- FUNCTIONS --- */" << std::endl;
            for (const auto& name : groupedEntities[NamedEntity::EntityType::Function]) {
                result << name << std::endl;
            }
            result << std::endl;
        }
        
        // Output variables
        if (!groupedEntities[NamedEntity::EntityType::Variable].empty()) {
            result << "/* --- VARIABLES --- */" << std::endl;
            for (const auto& name : groupedEntities[NamedEntity::EntityType::Variable]) {
                result << name << std::endl;
            }
            result << std::endl;
        }
        
        // Output enums
        if (!groupedEntities[NamedEntity::EntityType::Enum].empty()) {
            result << "/* --- ENUMS --- */" << std::endl;
            for (const auto& name : groupedEntities[NamedEntity::EntityType::Enum]) {
                result << name << std::endl;
            }
            result << std::endl;
        }
        
        // Output imports
        if (!groupedEntities[NamedEntity::EntityType::Import].empty()) {
            result << "/* --- IMPORTS/INCLUDES --- */" << std::endl;
            for (const auto& name : groupedEntities[NamedEntity::EntityType::Import]) {
                result << name << std::endl;
            }
            result << std::endl;
        }
        
        // Output other entities
        if (!groupedEntities[NamedEntity::EntityType::Other].empty()) {
            result << "/* --- OTHER --- */" << std::endl;
            for (const auto& name : groupedEntities[NamedEntity::EntityType::Other]) {
                result << name << std::endl;
            }
            result << std::endl;
        }
    } else {
        // Just list all entities
        result << "/* --- NAMED ENTITIES --- */" << std::endl;
        for (const auto& entity : entities) {
            result << entity.name << std::endl;
        }
        result << std::endl;
    }
    
    return result.str();
}

/**
 * @brief Reads a file's contents using the most appropriate method
 * 
 * @param filePath Path to the file to read
 * @return std::string The file's contents
 * @throws std::runtime_error if file cannot be read
 * 
 * Automatically chooses between buffered reading and memory mapping
 * based on file size. Uses memory mapping for files larger than
 * MMAP_THRESHOLD (1MB) and buffered reading for smaller files.
 */
std::string FileProcessor::readFile(const fs::path& filePath) const {
    std::error_code ec;
    uintmax_t fileSize = fs::file_size(filePath, ec);
    
    if (ec) {
        throw std::runtime_error("Error getting file size for " + filePath.string() + ": " + ec.message());
    }
    
    if (fileSize == 0) {
        return "";
    }
    
    // Use memory mapping for large files
    if (fileSize > MMAP_THRESHOLD) {
        return readLargeFile(filePath, fileSize);
    }
    
    // For smaller files, use buffered reading
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filePath.string());
    }
    
    // Reserve the exact size we need to avoid reallocations
    std::string content;
    content.reserve(static_cast<size_t>(fileSize));
    
    // Use a buffer to read the file in chunks
    char buffer[FILE_BUFFER_SIZE];
    
    while (file) {
        file.read(buffer, FILE_BUFFER_SIZE);
        content.append(buffer, file.gcount());
    }
    
    return content;
}

/**
 * @brief Reads a large file using memory mapping
 * 
 * @param filePath Path to the file to read
 * @param fileSize Size of the file in bytes
 * @return std::string The file's contents
 * @throws std::runtime_error if memory mapping fails
 * 
 * Uses memory mapping for efficient reading of large files.
 * Properly handles cleanup of mapped memory and file descriptors.
 */
std::string FileProcessor::readLargeFile(const fs::path& filePath, uintmax_t fileSize) const {
    int fd = open(filePath.c_str(), O_RDONLY);
    if (fd == -1) {
        throw std::runtime_error("Failed to open file for memory mapping: " + filePath.string());
    }
    
    void* mapped = mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd); // Close file descriptor immediately, mmap keeps the file open
    
    if (mapped == MAP_FAILED) {
        throw std::runtime_error("Memory mapping failed for file: " + filePath.string());
    }
    
    // Copy the mapped memory to a string
    std::string content(static_cast<char*>(mapped), fileSize);
    
    // Unmap the file
    munmap(mapped, fileSize);
    
    return content;
}

/**
 * @brief Determines if a file is binary
 * 
 * @param filePath Path to the file to check
 * @return bool True if the file appears to be binary, false otherwise
 * 
 * Uses multiple heuristics to detect binary files:
 * 1. Checks file extension against known binary extensions
 * 2. Examines file content for null bytes and non-text characters
 * 3. Analyzes the ratio of text to binary content
 * 
 * The method is conservative and will return false if unsure.
 */
bool FileProcessor::isBinaryFile(const fs::path& filePath) const {
    // Get file extension (lowercase)
    std::string ext = filePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // Common binary file extensions
    static const std::unordered_set<std::string> binaryExtensions = {
        ".exe", ".dll", ".so", ".dylib", ".o", ".obj", ".a", ".lib", 
        ".bin", ".dat", ".db", ".sqlite", ".class", ".jar", ".pyc",
        ".pyo", ".zip", ".tar", ".gz", ".xz", ".bz2", ".7z", ".rar",
        ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".ico", ".mp3", 
        ".mp4", ".avi", ".mov", ".pdf", ".doc", ".docx", ".xls", ".xlsx"
    };
    
    // If it's a known binary extension, return true
    if (binaryExtensions.find(ext) != binaryExtensions.end()) {
        return true;
    }
    
    // Peek at the first few bytes to check for binary content
    try {
        std::string content;
        
        // Only read a small portion of the file to detect binary content
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            // If we can't open it, assume it's not binary
            return false;
        }
        
        char buffer[1024];
        file.read(buffer, sizeof(buffer));
        std::streamsize bytesRead = file.gcount();
        
        // Check for null bytes and other binary indicators
        int nullCount = 0;
        int textCount = 0;
        
        for (int i = 0; i < bytesRead; i++) {
            char c = buffer[i];
            if (c == 0) {
                nullCount++;
            } else if ((c >= 32 && c <= 126) || c == '\n' || c == '\r' || c == '\t') {
                textCount++;
            }
        }
        
        // If more than 10% are null bytes or if less than 80% are text chars, consider it binary
        double nullRatio = static_cast<double>(nullCount) / bytesRead;
        double textRatio = static_cast<double>(textCount) / bytesRead;
        
        return (nullRatio > 0.1 || textRatio < 0.8);
    } catch (...) {
        // If we can't read it, assume it's not binary
        return false;
    }
}

/**
 * @brief Extracts named entities from source code
 * 
 * @param content The source code content
 * @param filePath Path to the source file
 * @return std::vector<NamedEntity> Vector of extracted named entities
 * 
 * Uses the CodeNER system to extract named entities such as:
 * - Class names
 * - Function names
 * - Variable names
 * - Enum values
 * - Import statements
 * 
 * Entities are filtered based on summarization options.
 * The number of returned entities can be limited by maxEntities option.
 */
std::vector<FileProcessor::NamedEntity> FileProcessor::extractNamedEntities(const std::string& content, const fs::path& filePath) const {
    // If no NER is required or content is empty, return empty
    if (!performNER_ || content.empty()) {
        return {};
    }
    
    // Get CodeNER instance
    CodeNER* ner = getCodeNER();
    if (!ner) {
        std::cerr << "Warning: CodeNER not available, skipping entity extraction for " << filePath << std::endl;
        return {};
    }
    
    // Prepare result vector
    std::vector<NamedEntity> entities;
    
    try {
        // Extract entities using CodeNER - just pass the content
        auto nerEntities = ner->extractEntities(content, filePath.string());
        
        // Convert CodeNER entities to our format
        for (const auto& nerEntity : nerEntities) {
            NamedEntity entity;
            entity.name = nerEntity.name;
            
            // Map types using integer values (assuming EntityType is an enum with integer values)
            int typeValue = static_cast<int>(nerEntity.type);
            
            // Map based on enum value ranges
            if (typeValue == 0) {
                entity.type = NamedEntity::EntityType::Class;
            } else if (typeValue == 1) {
                entity.type = NamedEntity::EntityType::Function;
            } else if (typeValue == 2) {
                entity.type = NamedEntity::EntityType::Variable;
            } else if (typeValue == 3) {
                entity.type = NamedEntity::EntityType::Enum;
            } else if (typeValue == 4) {
                entity.type = NamedEntity::EntityType::Import;
            } else {
                entity.type = NamedEntity::EntityType::Other;
            }
            
            entities.push_back(entity);
        }
        
        // Filter entities based on summarization options
        if (!summarizationOptions_.includeClassNames) {
            entities.erase(std::remove_if(entities.begin(), entities.end(), 
                [](const NamedEntity& e) { return e.type == NamedEntity::EntityType::Class; }), 
                entities.end());
        }
        
        if (!summarizationOptions_.includeFunctionNames) {
            entities.erase(std::remove_if(entities.begin(), entities.end(), 
                [](const NamedEntity& e) { return e.type == NamedEntity::EntityType::Function; }), 
                entities.end());
        }
        
        if (!summarizationOptions_.includeVariableNames) {
            entities.erase(std::remove_if(entities.begin(), entities.end(), 
                [](const NamedEntity& e) { return e.type == NamedEntity::EntityType::Variable; }), 
                entities.end());
        }
        
        if (!summarizationOptions_.includeEnumValues) {
            entities.erase(std::remove_if(entities.begin(), entities.end(), 
                [](const NamedEntity& e) { return e.type == NamedEntity::EntityType::Enum; }), 
                entities.end());
        }
        
        if (!summarizationOptions_.includeImports) {
            entities.erase(std::remove_if(entities.begin(), entities.end(), 
                [](const NamedEntity& e) { return e.type == NamedEntity::EntityType::Import; }), 
                entities.end());
        }
        
        // Limit number of entities if needed
        if (entities.size() > static_cast<size_t>(summarizationOptions_.maxEntities) && 
            summarizationOptions_.maxEntities > 0) {
            entities.resize(summarizationOptions_.maxEntities);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error extracting entities from " << filePath << ": " << e.what() << std::endl;
    }
    
    return entities;
}

/**
 * @brief Process all files in a directory with parallel file collection
 * 
 * @param dir Directory to process
 * @param batchSize Size of batches when adding files to the queue
 * @return std::vector<ProcessedFile> Results of processing
 */
std::vector<FileProcessor::ProcessedFile> FileProcessor::processDirectoryParallel(const fs::path& dir, size_t batchSize) {
    // Clear any existing results and queues
    results_.clear();
    
    while (!fileQueue_.empty()) {
        fileQueue_.pop();
    }
    
    while (!directoryQueue_.empty()) {
        directoryQueue_.pop();
    }
    
    // Reset collection done flag
    dirCollectionDone_ = false;
    
    // Start parallel file collection
    collectFilesParallel(dir, batchSize);
    
    // Process files with worker threads
    const unsigned int numThreads = std::thread::hardware_concurrency();
    done_ = false;
    
    std::vector<std::thread> workers;
    bool threadsStarted = true;
    
    try {
        // Create worker threads
        for (unsigned int i = 0; i < numThreads; ++i) {
            try {
                workers.emplace_back(&FileProcessor::workerThread, this);
            } catch (const std::system_error& e) {
                std::cerr << "Warning: Could not create worker thread: " << e.what() << std::endl;
                threadsStarted = false;
                break;
            }
        }
        
        // Wait for all worker threads to finish
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    } catch (const std::system_error& e) {
        threadsStarted = false;
        
        // Clean up any threads that were created
        done_ = true;
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers.clear();
    }
    
    // If thread creation failed, process files sequentially
    if (!threadsStarted) {
        // Fallback to sequential processing
        while (!fileQueue_.empty()) {
            fs::path filePath;
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                if (fileQueue_.empty()) break;
                filePath = fileQueue_.front();
                fileQueue_.pop();
            }
            
            try {
                ProcessedFile result = processFile(filePath);
                if (result.processed || result.skipped) {
                    std::lock_guard<std::mutex> lock(resultsMutex_);
                    results_.push_back(std::move(result));
                }
            } catch (const std::exception& e) {
                std::cerr << "Error processing file " << filePath << ": " << e.what() << std::endl;
            }
        }
    }
    
    return results_;
}

/**
 * @brief Collects files in parallel from directories
 * 
 * @param dir Root directory to start collection from
 * @param batchSize Size of batches when adding files to the queue
 */
void FileProcessor::collectFilesParallel(const fs::path& dir, size_t batchSize) {
    // Add the root directory to the queue
    addDirectoryToQueue(dir);
    
    // Determine number of threads for file collection
    // Use hardware_concurrency but with a reasonable limit
    unsigned int numCollectorThreads = std::min(
        std::thread::hardware_concurrency(),
        static_cast<unsigned int>(4)  // Limit to 4 threads for file collection to avoid too much I/O contention
    );
    
    // Create collector threads
    std::vector<std::thread> collectorThreads;
    for (unsigned int i = 0; i < numCollectorThreads; ++i) {
        collectorThreads.emplace_back(&FileProcessor::fileCollectorWorker, this, batchSize);
    }
    
    // Wait for all collector threads to finish
    for (auto& thread : collectorThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

/**
 * @brief Add a directory to the directory queue for parallel processing
 * 
 * @param dir Directory to add
 */
void FileProcessor::addDirectoryToQueue(const fs::path& dir) {
    {
        std::lock_guard<std::mutex> lock(dirQueueMutex_);
        directoryQueue_.push(dir);
    }
    // Notify one waiting thread that a directory is available
    dirQueueCondition_.notify_one();
}

/**
 * @brief Worker function for parallel file collection
 * 
 * @param batchSize Size of batches when adding files to the queue
 */
void FileProcessor::fileCollectorWorker(size_t batchSize) {
    // Local vector to collect files before adding them to the global queue
    std::vector<fs::path> localFiles;
    localFiles.reserve(batchSize);
    
    while (true) {
        fs::path currentDir;
        
        // Try to get a directory from the queue
        {
            std::unique_lock<std::mutex> lock(dirQueueMutex_);
            
            // Wait until there's a directory or we're done
            dirQueueCondition_.wait(lock, [this] {
                return !directoryQueue_.empty() || dirCollectionDone_;
            });
            
            // If queue is empty and collection is done, exit
            if (directoryQueue_.empty()) {
                if (dirCollectionDone_) {
                    break;
                }
                continue;
            }
            
            // Get a directory to process
            currentDir = directoryQueue_.front();
            directoryQueue_.pop();
        }
        
        try {
            // First, handle files in the current directory (non-recursive)
            for (const auto& entry : fs::directory_iterator(currentDir)) {
                if (fs::is_regular_file(entry)) {
                    if (shouldProcessFile(entry.path())) {
                        localFiles.push_back(entry.path());
                        
                        // If batch size reached, add to global queue
                        if (localFiles.size() >= batchSize) {
                            std::lock_guard<std::mutex> lock(queueMutex_);
                            for (const auto& file : localFiles) {
                                fileQueue_.push(file);
                            }
                            localFiles.clear();
                        }
                    }
                } else if (fs::is_directory(entry)) {
                    // Add subdirectory to the queue for processing
                    addDirectoryToQueue(entry.path());
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error collecting files from directory " << currentDir << ": " << e.what() << std::endl;
        }
    }
    
    // Add any remaining files to the global queue
    if (!localFiles.empty()) {
        std::lock_guard<std::mutex> lock(queueMutex_);
        for (const auto& file : localFiles) {
            fileQueue_.push(file);
        }
    }
    
    // Signal that one collector thread is done
    // The last thread to finish will set dirCollectionDone_ to true
    static std::atomic<int> finishedThreads(0);
    if (++finishedThreads >= std::min(
            std::thread::hardware_concurrency(),
            static_cast<unsigned int>(4))) {
        dirCollectionDone_ = true;
        dirQueueCondition_.notify_all();
    }
}