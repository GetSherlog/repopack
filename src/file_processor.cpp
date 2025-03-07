#include "file_processor.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

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