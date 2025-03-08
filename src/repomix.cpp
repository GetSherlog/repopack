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
    
    // Set summarization options
    fileProcessor_->setSummarizationOptions(options_.summarization);
    
    // Initialize file scorer if selection strategy is Scoring
    if (options_.selectionStrategy == RepomixOptions::FileSelectionStrategy::Scoring) {
        fileScorer_ = std::make_unique<FileScorer>(options_.scoringConfig);
    }
    
    // Initialize the tokenizer if token counting is enabled
    if (options_.countTokens) {
        tokenizer_ = std::make_unique<Tokenizer>(options_.tokenEncoding);
    }
}

bool Repomix::run() {
    try {
        // Start overall timer
        startTime_ = std::chrono::steady_clock::now();
        
        // Register a job ID if we don't have one yet
        if (jobId_.empty()) {
            jobId_ = ProgressTracker::getInstance().registerJob();
        }
        
        if (options_.verbose) {
            std::cout << "Processing directory: " << options_.inputDir << std::endl;
        }

        std::cout << "Processing directory: " << options_.inputDir << std::endl;
        
        // Start processing timer
        auto processStart = std::chrono::steady_clock::now();
        
        // Process files based on selection strategy
        std::vector<FileProcessor::ProcessedFile> files;
        
        if (options_.selectionStrategy == RepomixOptions::FileSelectionStrategy::Scoring) {
            // Start scoring timer
            auto scoringStart = std::chrono::steady_clock::now();
            
            // Select files using scoring system
            auto selectedFiles = selectFilesUsingScoring(options_.inputDir);
            
            // End scoring timer
            auto scoringEnd = std::chrono::steady_clock::now();
            scoringDuration_ = std::chrono::duration_cast<std::chrono::milliseconds>(scoringEnd - scoringStart);
            
            if (options_.verbose) {
                std::cout << "Selected " << selectedFiles.size() << " files using scoring system" << std::endl;
                std::cout << "Scoring completed in " << scoringDuration_.count() << " ms" << std::endl;
            }
            
            // Process the selected files
            files = processSelectedFiles(selectedFiles);
        } else {
            // Process all files using the standard method with parallel collection
            files = fileProcessor_->processDirectory(options_.inputDir, true);
        }

        std::cout << "Files processed: " << files.size() << std::endl;
        
        // End processing timer
        auto processEnd = std::chrono::steady_clock::now();
        processingDuration_ = std::chrono::duration_cast<std::chrono::milliseconds>(processEnd - processStart);

        std::cout << "Processing duration: " << processingDuration_.count() << " ms" << std::endl;
        
        // Update statistics
        totalFiles_ = files.size();
        for (const auto& file : files) {
            totalLines_ += file.lineCount;
            totalBytes_ += file.byteSize;
        }
        
        // Start output timer
        auto outputStart = std::chrono::steady_clock::now();
        
        // Format the output
        outputContent_ = formatOutput(files);
        
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
        
        // End output timer
        auto outputEnd = std::chrono::steady_clock::now();
        outputDuration_ = std::chrono::duration_cast<std::chrono::milliseconds>(outputEnd - outputStart);
        
        // Count tokens if requested
        if (options_.countTokens) {
            auto tokenStart = std::chrono::steady_clock::now();
            countOutputTokens();
            auto tokenEnd = std::chrono::steady_clock::now();
            tokenizationDuration_ = std::chrono::duration_cast<std::chrono::milliseconds>(tokenEnd - tokenStart);
        }
        
        // End overall timer
        endTime_ = std::chrono::steady_clock::now();
        duration_ = std::chrono::duration_cast<std::chrono::milliseconds>(endTime_ - startTime_);
        
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }
}

std::string Repomix::getSummary() const {
    std::stringstream ss;
    ss << "Repository processing summary:" << std::endl;
    ss << "  Total files: " << totalFiles_ << std::endl;
    ss << "  Total lines: " << totalLines_ << std::endl;
    ss << "  Total bytes: " << totalBytes_ << " bytes" << std::endl;
    
    if (options_.showTiming) {
        ss << "  Processing time: " << processingDuration_.count() << " ms" << std::endl;
        if (options_.selectionStrategy == RepomixOptions::FileSelectionStrategy::Scoring) {
            ss << "  Scoring time: " << scoringDuration_.count() << " ms" << std::endl;
        }
        ss << "  Output generation time: " << outputDuration_.count() << " ms" << std::endl;
        if (options_.countTokens) {
            ss << "  Tokenization time: " << tokenizationDuration_.count() << " ms" << std::endl;
        }
        ss << "  Total time: " << duration_.count() << " ms" << std::endl;
    }
    
    if (options_.countTokens) {
        ss << "  Token count (" << getTokenizerName() << "): " << tokenCount_ << std::endl;
    }
    
    return ss.str();
}

std::string Repomix::getTimingInfo() const {
    std::stringstream ss;
    ss << "Timing Information:" << std::endl;
    ss << "- Total time: " << duration_.count() << "ms" << std::endl;
    
    ss << "- File processing time: " << processingDuration_.count() << "ms ("
       << (processingDuration_.count() * 100 / (duration_.count() ? duration_.count() : 1)) << "%)" << std::endl;
    
    if (options_.selectionStrategy == RepomixOptions::FileSelectionStrategy::Scoring) {
        ss << "- File scoring time: " << scoringDuration_.count() << "ms ("
           << (scoringDuration_.count() * 100 / (duration_.count() ? duration_.count() : 1)) << "%)" << std::endl;
    }
    
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
    
    // Process and format the files with parallel collection
    auto processedFiles = fileProcessor_->processDirectory(options_.inputDir, true);
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
    std::stringstream output;
    
    switch (options_.format) {
        case OutputFormat::Markdown: {
            output << "# Repository Summary\n\n";
            output << "| Files | Lines | Size |\n";
            output << "|-------|-------|------|\n";
            output << "| " << totalFiles_ << " | " << totalLines_ << " | " 
                    << (totalBytes_ / 1024) << " KB |\n\n";
            
            output << "## Directory Structure\n\n";
            output << "```\n";
            output << generateDirectoryTree(options_.inputDir);
            output << "```\n\n";
            
            output << "## File Contents\n\n";
            
            for (const auto& file : files) {
                std::string relPath = fs::relative(file.path, options_.inputDir).string();
                std::string extension = file.path.extension().string();
                
                output << "### " << relPath << "\n\n";
                
                // Output file statistics
                output << "*" << file.lineCount << " lines, " << (file.byteSize / 1024) << " KB*\n\n";
                
                // Special handling for README files when in Markdown format
                if (options_.summarization.includeReadme && 
                    options_.summarization.enabled && 
                    fileProcessor_->isReadmeFile(file.path)) {
                    output << file.content << "\n\n";
                    continue;
                }
                
                // Add file content with correct language highlighting in markdown
                output << "```";
                if (extension == ".cpp" || extension == ".hpp" || extension == ".h") {
                    output << "cpp";
                } else if (extension == ".js") {
                    output << "javascript";
                } else if (extension == ".py") {
                    output << "python";
                } else if (extension == ".ts") {
                    output << "typescript";
                } else if (extension == ".jsx" || extension == ".tsx") {
                    output << "jsx";
                } else if (extension == ".html") {
                    output << "html";
                } else if (extension == ".css") {
                    output << "css";
                } else if (extension == ".json") {
                    output << "json";
                } else if (extension == ".md") {
                    output << "markdown";
                } else if (extension == ".sh") {
                    output << "bash";
                }
                output << "\n";
                
                // Apply summarization if enabled and file is large
                if (options_.summarization.enabled && file.byteSize > options_.summarization.fileSizeThreshold) {
                    output << fileProcessor_->summarizeFile(file);
                } else {
                    output << file.content;
                }
                
                output << "```\n\n";
            }
            break;
        }
        
        case OutputFormat::XML: {
            output << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
            output << "<repository>\n";
            output << "  <summary>\n";
            output << "    <files>" << totalFiles_ << "</files>\n";
            output << "    <lines>" << totalLines_ << "</lines>\n";
            output << "    <size>" << totalBytes_ << "</size>\n";
            output << "  </summary>\n";
            
            // Directory structure representation
            output << "  <directory_structure><![CDATA[\n";
            output << generateDirectoryTree(options_.inputDir);
            output << "]]></directory_structure>\n";
            
            output << "  <files>\n";
            
            for (const auto& file : files) {
                std::string relPath = fs::relative(file.path, options_.inputDir).string();
                
                output << "    <file>\n";
                output << "      <path>" << relPath << "</path>\n";
                output << "      <lines>" << file.lineCount << "</lines>\n";
                output << "      <size>" << file.byteSize << "</size>\n";
                output << "      <content><![CDATA[";
                
                // Apply summarization if enabled and file is large
                if (options_.summarization.enabled && file.byteSize > options_.summarization.fileSizeThreshold) {
                    output << fileProcessor_->summarizeFile(file);
                } else {
                    output << file.content;
                }
                
                output << "]]></content>\n";
                output << "    </file>\n";
            }
            
            output << "  </files>\n";
            output << "</repository>\n";
            break;
        }
        
        case OutputFormat::ClaudeXML: {
            // Format for Claude according to the recommended XML structure
            // Long-form data at the top, structured with XML tags
            output << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
            output << "<documents>\n";
            
            int documentIndex = 1;
            for (const auto& file : files) {
                std::string relPath = fs::relative(file.path, options_.inputDir).string();
                
                output << "  <document index=\"" << documentIndex++ << "\">\n";
                output << "    <source>" << relPath << "</source>\n";
                output << "    <document_content>\n";
                
                // Apply summarization if enabled and file is large
                if (options_.summarization.enabled && file.byteSize > options_.summarization.fileSizeThreshold) {
                    output << fileProcessor_->summarizeFile(file);
                } else {
                    output << file.content;
                }
                
                output << "    </document_content>\n";
                output << "  </document>\n";
            }
            
            output << "</documents>\n\n";
            
            // Add repository summary at the end
            output << "Repository: " << options_.inputDir.filename().string() << "\n";
            output << "Files: " << totalFiles_ << " | Lines: " << totalLines_ << " | Size: " << totalBytes_ << " bytes\n\n";
            
            // Add a starter prompt instruction for Claude
            output << "Analyze the codebase. Identify key components, architecture, and main functionality.\n";
            output << "When referring to specific parts of the code, please quote the relevant sections.\n";
            break;
        }
        
        case OutputFormat::Plain:
        default: {
            output << "Repository Summary\n";
            output << "==================\n";
            output << "Files: " << totalFiles_ << "\n";
            output << "Lines: " << totalLines_ << "\n";
            output << "Size: " << (totalBytes_ / 1024) << " KB\n\n";
            
            output << "Directory Structure\n";
            output << "------------------\n";
            output << generateDirectoryTree(options_.inputDir) << "\n\n";
            
            output << "File Contents\n";
            output << "-------------\n";
            
            for (const auto& file : files) {
                std::string relPath = fs::relative(file.path, options_.inputDir).string();
                
                output << "=== " << relPath << " ===\n";
                output << "Lines: " << file.lineCount << ", Size: " << (file.byteSize / 1024) << " KB\n";
                
                // Apply summarization if enabled and file is large
                if (options_.summarization.enabled && file.byteSize > options_.summarization.fileSizeThreshold) {
                    output << fileProcessor_->summarizeFile(file);
                } else {
                    output << file.content;
                }
                
                output << "\n\n";
            }
            break;
        }
    }
    
    return output.str();
}

// Get the output content directly (useful for WASM)
std::string Repomix::getOutput() const {
    return outputContent_;
}

void Repomix::countOutputTokens() {
    if (!tokenizer_) {
        tokenizer_ = std::make_unique<Tokenizer>(options_.tokenEncoding);
    }
    
    tokenCount_ = tokenizer_->countTokens(outputContent_);
}

size_t Repomix::getTokenCount() const {
    return tokenCount_;
}

std::string Repomix::getTokenizerName() const {
    return tokenizer_ ? tokenizer_->getEncodingName() : "None";
}

// New method to get file scoring report
std::string Repomix::getFileScoringReport() const {
    if (!fileScorer_ || scoredFiles_.empty()) {
        return "No file scoring data available.";
    }
    
    return fileScorer_->getScoringReport(scoredFiles_);
}

std::vector<fs::path> Repomix::selectFilesUsingScoring(const fs::path& repoPath) {
    if (!fileScorer_) {
        throw std::runtime_error("File scorer not initialized");
    }
    
    // Score all files in the repository
    scoredFiles_ = fileScorer_->scoreRepository(repoPath);
    
    // Get selected files based on scoring
    return fileScorer_->getSelectedFiles(scoredFiles_);
}

std::vector<FileProcessor::ProcessedFile> Repomix::processSelectedFiles(const std::vector<fs::path>& selectedFiles) {
    std::vector<FileProcessor::ProcessedFile> processedFiles;
    
    // Process each selected file
    for (const auto& filePath : selectedFiles) {
        try {
            auto processedFile = fileProcessor_->processFile(filePath);
            processedFiles.push_back(std::move(processedFile));
        }
        catch (const std::exception& e) {
            if (options_.verbose) {
                std::cerr << "Error processing file " << filePath << ": " << e.what() << std::endl;
            }
        }
    }
    
    return processedFiles;
}

/**
 * @brief Sets a callback for progress updates
 * 
 * @param callback Function to call with progress information
 */
void Repomix::setProgressCallback(ProgressCallback callback) {
    // If fileProcessor is initialized, set the callback
    if (fileProcessor_) {
        fileProcessor_->setProgressCallback([this, callback](const FileProcessor::ProgressInfo& progress) {
            // Forward the progress to the provided callback
            if (callback) {
                callback(progress);
            }
            
            // Also update the progress tracker if we have a job ID
            if (!jobId_.empty()) {
                ProgressTracker::getInstance().updateProgress(jobId_, progress);
            }
        });
    }
}

/**
 * @brief Get current progress information
 * 
 * @return FileProcessor::ProgressInfo Current progress data
 */
FileProcessor::ProgressInfo Repomix::getCurrentProgress() const {
    if (fileProcessor_) {
        return fileProcessor_->getCurrentProgress();
    }
    
    // Return empty progress info if no file processor
    return FileProcessor::ProgressInfo();
}

/**
 * @brief Set the job ID for this Repomix instance
 * 
 * @param jobId The job ID to associate with this instance
 */
void Repomix::setJobId(const std::string& jobId) {
    jobId_ = jobId;
}

/**
 * @brief Get the job ID for this Repomix instance
 * 
 * @return std::string The job ID
 */
std::string Repomix::getJobId() const {
    return jobId_;
}