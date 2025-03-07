#include "repomix.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>

Repomix::Repomix(const RepomixOptions& options) 
    : options_(options) {
    
    // Initialize the pattern matcher
    patternMatcher_ = std::make_unique<PatternMatcher>();
    
    // Check for .gitignore file in input directory
    const auto gitignorePath = options_.inputDir / ".gitignore";
    if (fs::exists(gitignorePath)) {
        patternMatcher_->loadGitignore(gitignorePath);
    }
    
    // Add default ignore patterns
    
    // Version control
    patternMatcher_->addIgnorePattern(".git/**");
    patternMatcher_->addIgnorePattern(".svn/**");
    patternMatcher_->addIgnorePattern(".hg/**");
    
    // Build directories
    patternMatcher_->addIgnorePattern("build/**");
    patternMatcher_->addIgnorePattern("dist/**");
    patternMatcher_->addIgnorePattern("out/**");
    patternMatcher_->addIgnorePattern("target/**");
    patternMatcher_->addIgnorePattern("bin/**");
    patternMatcher_->addIgnorePattern("obj/**");
    
    // Dependencies
    patternMatcher_->addIgnorePattern("node_modules/**");
    patternMatcher_->addIgnorePattern("vendor/**");
    patternMatcher_->addIgnorePattern("bower_components/**");
    patternMatcher_->addIgnorePattern("jspm_packages/**");
    patternMatcher_->addIgnorePattern("packages/**");
    patternMatcher_->addIgnorePattern("_deps/**");
    
    // Docker
    patternMatcher_->addIgnorePattern("Dockerfile");
    patternMatcher_->addIgnorePattern("docker-compose.yml");
    patternMatcher_->addIgnorePattern(".dockerignore");
    
    // Caches
    patternMatcher_->addIgnorePattern(".cache/**");
    patternMatcher_->addIgnorePattern("__pycache__/**");
    patternMatcher_->addIgnorePattern(".pytest_cache/**");
    patternMatcher_->addIgnorePattern(".nyc_output/**");
    
    // IDE and editor files
    patternMatcher_->addIgnorePattern(".idea/**");
    patternMatcher_->addIgnorePattern(".vscode/**");
    patternMatcher_->addIgnorePattern("*.sublime-*");
    patternMatcher_->addIgnorePattern("*.swp");
    patternMatcher_->addIgnorePattern(".DS_Store");
    
    // Binary and compiled files
    patternMatcher_->addIgnorePattern("*.exe");
    patternMatcher_->addIgnorePattern("*.dll");
    patternMatcher_->addIgnorePattern("*.so");
    patternMatcher_->addIgnorePattern("*.dylib");
    patternMatcher_->addIgnorePattern("*.a");
    patternMatcher_->addIgnorePattern("*.lib");
    patternMatcher_->addIgnorePattern("*.o");
    patternMatcher_->addIgnorePattern("*.obj");
    patternMatcher_->addIgnorePattern("*.class");
    patternMatcher_->addIgnorePattern("*.jar");
    patternMatcher_->addIgnorePattern("*.war");
    patternMatcher_->addIgnorePattern("*.pyc");
    patternMatcher_->addIgnorePattern("*.pyo");
    
    // Package files
    patternMatcher_->addIgnorePattern("*.zip");
    patternMatcher_->addIgnorePattern("*.tar.gz");
    patternMatcher_->addIgnorePattern("*.tgz");
    patternMatcher_->addIgnorePattern("*.rar");
    patternMatcher_->addIgnorePattern("*.7z");
    
    // Logs
    patternMatcher_->addIgnorePattern("*.log");
    patternMatcher_->addIgnorePattern("logs/**");
    
    // Generated files
    patternMatcher_->addIgnorePattern("CMakeFiles/**");
    patternMatcher_->addIgnorePattern("CMakeCache.txt");
    patternMatcher_->addIgnorePattern("cmake_install.cmake");
    
    // Apply include patterns if specified
    if (!options_.includePatterns.empty()) {
        patternMatcher_->setIncludePatterns(options_.includePatterns);
        
        if (options_.verbose) {
            std::cout << "Using include patterns: " << options_.includePatterns << std::endl;
        }
    }
    
    // Apply exclude patterns if specified
    if (!options_.excludePatterns.empty()) {
        patternMatcher_->setExcludePatterns(options_.excludePatterns);
        
        if (options_.verbose) {
            std::cout << "Using exclude patterns: " << options_.excludePatterns << std::endl;
        }
    }
    
    // Create file processor with pattern matcher
    fileProcessor_ = std::make_unique<FileProcessor>(*patternMatcher_, options_.numThreads);
}

bool Repomix::run() {
    try {
        // Start overall timer
        startTime_ = std::chrono::steady_clock::now();
        
        if (options_.verbose) {
            std::cout << "Processing directory: " << options_.inputDir << std::endl;
        }
        
        // Start processing timer
        auto processStart = std::chrono::steady_clock::now();
        
        // Process directory
        const auto files = fileProcessor_->processDirectory(options_.inputDir);
        
        // End processing timer
        auto processEnd = std::chrono::steady_clock::now();
        processingDuration_ = std::chrono::duration_cast<std::chrono::milliseconds>(processEnd - processStart);
        
        // Update statistics
        totalFiles_ = files.size();
        for (const auto& file : files) {
            totalLines_ += file.lineCount;
            totalBytes_ += file.byteSize;
        }
        
        // Start output timer
        auto outputStart = std::chrono::steady_clock::now();
        
        // Write output
        writeOutput();
        
        // End output timer
        auto outputEnd = std::chrono::steady_clock::now();
        outputDuration_ = std::chrono::duration_cast<std::chrono::milliseconds>(outputEnd - outputStart);
        
        // End overall timer
        endTime_ = std::chrono::steady_clock::now();
        duration_ = std::chrono::duration_cast<std::chrono::milliseconds>(endTime_ - startTime_);
        
        if (options_.verbose) {
            std::cout << "Completed in " << duration_.count() << "ms" << std::endl;
            std::cout << "Output written to " << options_.outputFile << std::endl;
        }
        
        if (options_.showTiming) {
            std::cout << getTimingInfo() << std::endl;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }
}

std::string Repomix::getSummary() const {
    std::stringstream ss;
    ss << "Repository Summary:" << std::endl;
    ss << "- Total files: " << totalFiles_ << std::endl;
    ss << "- Total lines: " << totalLines_ << std::endl;
    ss << "- Total bytes: " << totalBytes_ << std::endl;
    return ss.str();
}

std::string Repomix::getTimingInfo() const {
    std::stringstream ss;
    ss << "Timing Information:" << std::endl;
    ss << "- Total time: " << duration_.count() << "ms" << std::endl;
    
    ss << "- File processing time: " << processingDuration_.count() << "ms ("
       << (processingDuration_.count() * 100 / (duration_.count() ? duration_.count() : 1)) << "%)" << std::endl;
    
    ss << "- Output generation time: " << outputDuration_.count() << "ms ("
       << (outputDuration_.count() * 100 / (duration_.count() ? duration_.count() : 1)) << "%)" << std::endl;
    
    // Calculate overhead time (initialization, memory allocation, etc.)
    auto overheadTime = duration_ - processingDuration_ - outputDuration_;
    
    ss << "- Overhead time: " << overheadTime.count() << "ms ("
       << (overheadTime.count() * 100 / (duration_.count() ? duration_.count() : 1)) << "%)" << std::endl;
    
    // Add some basic performance metrics
    if (processingDuration_.count() > 0) {
        double filesPerSecond = static_cast<double>(totalFiles_) / (processingDuration_.count() / 1000.0);
        double kbPerSecond = static_cast<double>(totalBytes_) / 1024.0 / (processingDuration_.count() / 1000.0);
        double linesPerSecond = static_cast<double>(totalLines_) / (processingDuration_.count() / 1000.0);
        
        ss << "- Performance:" << std::endl;
        ss << "  * " << std::fixed << std::setprecision(2) << filesPerSecond << " files/second" << std::endl;
        ss << "  * " << std::fixed << std::setprecision(2) << linesPerSecond << " lines/second" << std::endl;
        ss << "  * " << std::fixed << std::setprecision(2) << kbPerSecond << " KB/second" << std::endl;
    }
    
    return ss.str();
}

void Repomix::writeOutput() {
    // Create a stringstream for the content
    std::stringstream ss;
    
    // Write directory tree
    ss << "Repository: " << options_.inputDir.filename().string() << "\n";
    
    // Format the current time
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    ss << "Processed at: " << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S") << "\n";
    
    ss << "Files: " << totalFiles_ << "\n";
    ss << "Lines: " << totalLines_ << "\n";
    ss << "Size: " << totalBytes_ << " bytes\n\n";
    
    // Directory structure
    ss << "Directory structure:\n";
    ss << generateDirectoryTree(options_.inputDir) << "\n\n";
    
    // Process and format the files
    auto processedFiles = fileProcessor_->processDirectory(options_.inputDir);
    ss << formatOutput(processedFiles);
    
    // Store the content
    outputContent_ = ss.str();
    
    // Write to file if specified
    if (!options_.outputFile.empty()) {
        std::ofstream outFile(options_.outputFile);
        if (outFile) {
            outFile << outputContent_;
            if (options_.verbose) {
                std::cout << "Output written to " << options_.outputFile << std::endl;
            }
        } else {
            std::cerr << "Error: Could not open output file: " << options_.outputFile << std::endl;
        }
    }
}

std::string Repomix::generateDirectoryTree(const fs::path& dir, int level) const {
    // Preallocate a reasonably sized buffer
    std::string result;
    result.reserve(4096); // Start with 4KB, will grow if needed
    
    // Create indentation strings once
    static const std::string indentChars = "  ";
    std::string indent;
    for (int i = 0; i < level; ++i) {
        indent += indentChars;
    }
    
    for (const auto& entry : fs::directory_iterator(dir)) {
        // Skip ignored entries
        if (patternMatcher_->isIgnored(entry.path())) {
            continue;
        }
        
        // Add indentation and entry
        result += indent;
        result += entry.is_directory() ? "📁 " : "📄 ";
        result += entry.path().filename().string();
        result += "\n";
        
        // Recursively process subdirectories
        if (entry.is_directory()) {
            result += generateDirectoryTree(entry.path(), level + 1);
        }
    }
    
    return result;
}

std::string Repomix::formatOutput(const std::vector<FileProcessor::ProcessedFile>& files) const {
    // Calculate initial buffer size based on all file contents plus overhead
    size_t estimatedSize = 0;
    for (const auto& file : files) {
        // Add file content size plus overhead for formatting (headers, etc.)
        estimatedSize += file.content.size() + file.path.string().size() + 100;
    }
    
    // Prepare buffer with estimated size
    std::string result;
    result.reserve(estimatedSize);
    
    switch (options_.format) {
        case OutputFormat::Markdown:
            result += "## File Contents\n";
            for (const auto& file : files) {
                result += "### " + file.path.string() + "\n";
                result += "```\n";
                result += file.content + "\n";
                result += "```\n\n";
            }
            break;
            
        case OutputFormat::XML:
            result += "<repository>\n";
            for (const auto& file : files) {
                result += "  <file path=\"" + file.path.string() + "\">\n";
                result += "    <![CDATA[" + file.content + "]]>\n";
                result += "  </file>\n";
            }
            result += "</repository>\n";
            break;
            
        case OutputFormat::Plain:
        default:
            result += "## File Contents\n";
            for (const auto& file : files) {
                result += "--- " + file.path.string() + " ---\n";
                result += file.content + "\n\n";
            }
            break;
    }
    
    return result;
}

// Get the output content directly (useful for WASM)
std::string Repomix::getOutput() const {
    return outputContent_;
}