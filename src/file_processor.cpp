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
#include "../include/file_processor.hpp"
#include "../include/code_ner.hpp"
#include "repomix.hpp"  // For SummarizationOptions

// Size threshold for using memory mapping (files larger than this will use memory mapping)
constexpr size_t MMAP_THRESHOLD = 1 * 1024 * 1024; // 1 MB

FileProcessor::FileProcessor(const PatternMatcher& patternMatcher, unsigned int numThreads)
    : patternMatcher_(patternMatcher), 
      numThreads_(numThreads == 0 ? 1 : numThreads),
      done_(false) {
}

FileProcessor::~FileProcessor() {
    // Make sure threads are joined
    done_ = true;
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

std::vector<FileProcessor::ProcessedFile> FileProcessor::processDirectory(const fs::path& dir) {
    // Validate directory
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        throw std::runtime_error("Invalid directory: " + dir.string());
    }
    
    // Reset state
    done_ = false;
    fileQueue_ = std::queue<fs::path>();
    results_.clear();
    
    // Collect all files to process first (allows better work distribution)
    collectFiles(dir);
    
    // If we have files to process, start worker threads
    if (!fileQueue_.empty()) {
        // Create worker threads (use at most numThreads_ or fileQueue_.size() threads)
        unsigned int actualThreads = std::min(numThreads_, static_cast<unsigned int>(fileQueue_.size()));
        workers_.clear();
        
        bool threadsStarted = true;
        
        try {
            // Start with just 1 thread, then add more if successful
            workers_.emplace_back(&FileProcessor::workerThread, this);
            
            // If first thread creation succeeded, try to add more threads
            if (actualThreads > 1) {
                for (unsigned int i = 1; i < actualThreads; ++i) {
                    try {
                        workers_.emplace_back(&FileProcessor::workerThread, this);
                    } catch (const std::system_error& e) {
                        // If we can't create more threads, just use what we have
                        std::cerr << "Warning: Could not create additional thread: " << e.what() << std::endl;
                        break;
                    }
                }
            }
            
            // Wait for all worker threads to finish
            for (auto& worker : workers_) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
        } catch (const std::system_error& e) {
            // Handle thread creation failure (common in WASM environment)
            std::cerr << "Warning: Thread creation failed: " << e.what() << std::endl;
            std::cerr << "Falling back to single-threaded processing" << std::endl;
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

void FileProcessor::collectFiles(const fs::path& dir) {
    try {
        // Recursively iterate through directory
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            // Skip directories
            if (entry.is_directory()) {
                continue;
            }
            
            // Skip files that match ignore patterns
            const auto& path = entry.path();
            if (!shouldProcessFile(path)) {
                continue;
            }
            
            // Add to queue
            fileQueue_.push(path);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Warning: Failed to collect files from " << dir << ": " << e.what() << std::endl;
    }
}

void FileProcessor::workerThread() {
    while (!done_) {
        // Get a file to process from the queue
        fs::path filePath;
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            if (fileQueue_.empty()) {
                break;
            }
            
            filePath = fileQueue_.front();
            fileQueue_.pop();
        }
        
        // Process the file
        try {
            ProcessedFile processedFile;
            
            // Check if we should use memory mapping for this file
            if (shouldUseMemoryMapping(filePath)) {
                processedFile = processFileWithMemoryMapping(filePath);
            } else {
                processedFile = processFile(filePath);
            }
            
            // Add result to the result vector
            {
                std::lock_guard<std::mutex> lock(resultsMutex_);
                results_.push_back(std::move(processedFile));
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Warning: Failed to process file " << filePath << ": " << e.what() << std::endl;
        }
    }
}

FileProcessor::ProcessedFile FileProcessor::processFile(const fs::path& filePath) {
    // Validate file
    if (!fs::exists(filePath) || !fs::is_regular_file(filePath)) {
        throw std::runtime_error("Invalid file: " + filePath.string());
    }
    
    // Read file content
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filePath.string());
    }
    
    // Preallocate string based on file size
    const auto fileSize = fs::file_size(filePath);
    std::string content;
    content.reserve(fileSize);
    
    // Read file content into string using a larger buffer for better performance
    constexpr size_t bufferSize = 16384; // 16 KB buffer
    char buffer[bufferSize];
    
    while (file) {
        file.read(buffer, bufferSize);
        content.append(buffer, file.gcount());
    }
    
    // Create processed file
    ProcessedFile result;
    result.path = filePath;
    result.content = std::move(content);
    result.lineCount = countLines(result.content);
    result.byteSize = result.content.size();
    
    return result;
}

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

size_t FileProcessor::countLines(const std::string& content) const {
    // Optimized line counting using std::count
    size_t count = std::count(content.begin(), content.end(), '\n');
    
    // If the last line doesn't end with a newline, count it too
    if (!content.empty() && content.back() != '\n') {
        ++count;
    }
    
    return count;
}

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

// Add implementation for setting summarization options
void FileProcessor::setSummarizationOptions(const SummarizationOptions& options) {
    summarizationOptions_ = options;
}

// Determine if a file should be summarized based on size and settings
bool FileProcessor::shouldSummarizeFile(const ProcessedFile& file) const {
    if (!summarizationOptions_.enabled) {
        return false;
    }
    
    return file.byteSize > summarizationOptions_.fileSizeThreshold;
}

// Main summarization method
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
    
    std::vector<std::string> summaryLines;
    
    // Extract and add named entities first if enabled
    if (summarizationOptions_.includeEntityRecognition) {
        CodeNER* nerSystem = getCodeNER();
        if (nerSystem) {
            auto entities = nerSystem->extractEntities(file.content, file.path);
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

// Extract the first N lines from a string
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

// Extract function and class signatures using regex or tree-sitter
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

// Extract docstrings and important comments
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

// Extract representative snippets from different parts of the file
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

// Check if a file is a README file
bool FileProcessor::isReadmeFile(const fs::path& filePath) const {
    std::string filename = filePath.filename().string();
    std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
    
    return (filename == "readme.md" || filename == "readme.txt" || 
            filename == "readme" || filename.find("readme.") == 0);
}

// Code NER access method
CodeNER* FileProcessor::getCodeNER() const {
    // Lazy initialization of the CodeNER instance
    if (!codeNER_ && summarizationOptions_.includeEntityRecognition) {
        codeNER_ = CodeNER::create(summarizationOptions_);
    }
    return codeNER_.get();
}

// Format entities as a string
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