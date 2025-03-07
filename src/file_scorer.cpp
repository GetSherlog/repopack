#include "file_scorer.hpp"
#include <fstream>
#include <regex>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <numeric>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <cmath>
#include <nlohmann/json.hpp>
#include <tree_sitter/api.h>
#include <set>

// Include language headers for TreeSitter
extern "C" {
    TSLanguage* tree_sitter_cpp();
    TSLanguage* tree_sitter_c();
    TSLanguage* tree_sitter_python();
    TSLanguage* tree_sitter_javascript();
}

namespace fs = std::filesystem;

// For convenience
using json = nlohmann::json;

FileScorer::FileScorer(const FileScoringConfig& config)
    : config_(config) {
    // Initialize pattern matcher
    patternMatcher_ = std::make_unique<PatternMatcher>();
}

void FileScorer::setConfig(const FileScoringConfig& config) {
    config_ = config;
}

const FileScoringConfig& FileScorer::getConfig() const {
    return config_;
}

std::vector<FileScorer::ScoredFile> FileScorer::scoreRepository(const fs::path& repoPath) {
    if (!fs::exists(repoPath) || !fs::is_directory(repoPath)) {
        throw std::runtime_error("Invalid repository path: " + repoPath.string());
    }

    std::vector<ScoredFile> scoredFiles;
    
    // Build dependency graph if needed for connectivity scoring
    std::unordered_map<std::string, std::vector<std::string>> dependencyGraph;
    if (config_.dependencyGraphWeight > 0.0f) {
        dependencyGraph = buildDependencyGraph(repoPath);
    }
    
    // Iterate through the repository and score each file
    for (const auto& entry : fs::recursive_directory_iterator(repoPath)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        
        try {
            // Score the file
            auto scoredFile = scoreFile(entry.path(), repoPath);
            
            // Add dependency graph score if applicable
            if (config_.dependencyGraphWeight > 0.0f) {
                float connectivityScore = calculateConnectivityScore(entry.path(), dependencyGraph);
                scoredFile.componentScores["connectivity"] = connectivityScore * config_.dependencyGraphWeight;
                scoredFile.score += scoredFile.componentScores["connectivity"];
            }
            
            // Normalize score to 0-1 range
            scoredFile.score = std::min(1.0f, std::max(0.0f, scoredFile.score));
            
            // Determine inclusion based on score threshold
            scoredFile.included = scoredFile.score >= config_.inclusionThreshold;
            
            scoredFiles.push_back(scoredFile);
        }
        catch (const std::exception& e) {
            std::cerr << "Error scoring file " << entry.path() << ": " << e.what() << std::endl;
        }
    }
    
    // Sort the files by score (highest first)
    std::sort(scoredFiles.begin(), scoredFiles.end(), 
        [](const ScoredFile& a, const ScoredFile& b) {
            return a.score > b.score;
        });
    
    return scoredFiles;
}

FileScorer::ScoredFile FileScorer::scoreFile(const fs::path& filePath, const fs::path& repoRoot) {
    ScoredFile result;
    result.path = filePath;
    result.score = 0.0f;
    
    // Get relative path from repo root
    fs::path relPath = fs::relative(filePath, repoRoot);
    
    // Calculate individual score components
    float structureScore = scoreProjectStructure(filePath, repoRoot);
    float typeScore = scoreFileType(filePath);
    float recencyScore = scoreRecency(filePath);
    float sizeScore = scoreFileSize(filePath);
    float densityScore = scoreCodeDensity(filePath);
    
    // Store component scores for reporting
    result.componentScores["structure"] = structureScore;
    result.componentScores["file_type"] = typeScore;
    result.componentScores["recency"] = recencyScore;
    result.componentScores["size"] = sizeScore;
    result.componentScores["density"] = densityScore;
    
    // Calculate total score
    result.score = structureScore + typeScore + recencyScore + sizeScore + densityScore;
    
    return result;
}

std::vector<fs::path> FileScorer::getSelectedFiles(const std::vector<ScoredFile>& scoredFiles) {
    std::vector<fs::path> selectedFiles;
    
    for (const auto& file : scoredFiles) {
        if (file.included) {
            selectedFiles.push_back(file.path);
        }
    }
    
    return selectedFiles;
}

std::string FileScorer::getScoringReport(const std::vector<ScoredFile>& scoredFiles) const {
    json report;
    
    // Add scoring config
    json configJson;
    configJson["rootFilesWeight"] = config_.rootFilesWeight;
    configJson["topLevelDirsWeight"] = config_.topLevelDirsWeight;
    configJson["entryPointsWeight"] = config_.entryPointsWeight;
    configJson["dependencyGraphWeight"] = config_.dependencyGraphWeight;
    configJson["sourceCodeWeight"] = config_.sourceCodeWeight;
    configJson["configFilesWeight"] = config_.configFilesWeight;
    configJson["documentationWeight"] = config_.documentationWeight;
    configJson["testFilesWeight"] = config_.testFilesWeight;
    configJson["recentlyModifiedWeight"] = config_.recentlyModifiedWeight;
    configJson["recentTimeWindowDays"] = config_.recentTimeWindowDays;
    configJson["fileSizeWeight"] = config_.fileSizeWeight;
    configJson["largeFileThreshold"] = config_.largeFileThreshold;
    configJson["codeDensityWeight"] = config_.codeDensityWeight;
    configJson["inclusionThreshold"] = config_.inclusionThreshold;
    configJson["useTreeSitter"] = config_.useTreeSitter;
    report["config"] = configJson;
    
    // Add file scores
    json filesJson = json::array();
    for (const auto& file : scoredFiles) {
        json fileJson;
        fileJson["path"] = file.path.string();
        fileJson["score"] = file.score;
        fileJson["included"] = file.included;
        
        // Add component scores
        json componentsJson;
        for (const auto& [key, value] : file.componentScores) {
            componentsJson[key] = value;
        }
        fileJson["components"] = componentsJson;
        
        filesJson.push_back(fileJson);
    }
    report["files"] = filesJson;
    
    // Add summary statistics
    size_t totalFiles = scoredFiles.size();
    size_t includedFiles = 0;
    
    for (const auto& file : scoredFiles) {
        if (file.included) {
            includedFiles++;
        }
    }
    
    report["summary"] = {
        {"total_files", totalFiles},
        {"included_files", includedFiles},
        {"inclusion_percentage", totalFiles > 0 ? (static_cast<float>(includedFiles) / totalFiles) * 100.0f : 0.0f}
    };
    
    return report.dump(2); // Pretty-print with 2-space indentation
}

float FileScorer::scoreProjectStructure(const fs::path& filePath, const fs::path& repoRoot) {
    fs::path relPath = fs::relative(filePath, repoRoot);
    std::string pathStr = relPath.string();
    float score = 0.0f;
    
    // Boost score for root-level files
    if (relPath.parent_path().empty()) {
        score += config_.rootFilesWeight;
        
        // Extra boost for important root files (README, package.json, etc.)
        if (matchesAnyPattern(relPath.filename().string(), config_.importantFilePatterns)) {
            score += config_.rootFilesWeight * 0.5f;
        }
    }
    
    // Boost score for files in important top-level directories
    for (const auto& dirPattern : config_.importantDirPatterns) {
        std::regex dirRegex(dirPattern);
        if (std::regex_search(pathStr, dirRegex)) {
            score += config_.topLevelDirsWeight;
            break;
        }
    }
    
    // Boost score for entry points
    if (isEntryPoint(filePath)) {
        score += config_.entryPointsWeight;
    }
    
    return score;
}

float FileScorer::scoreFileType(const fs::path& filePath) {
    float score = 0.0f;
    
    // Score based on file type
    if (isSourceCodeFile(filePath)) {
        score += config_.sourceCodeWeight;
    }
    else if (isConfigFile(filePath)) {
        score += config_.configFilesWeight;
    }
    else if (isDocumentationFile(filePath)) {
        score += config_.documentationWeight;
    }
    
    // Apply penalty for test files (unless they're explicitly valued)
    if (isTestFile(filePath)) {
        score = config_.testFilesWeight;  // Replace previous score
    }
    
    return score;
}

float FileScorer::scoreRecency(const fs::path& filePath) {
    if (config_.recentlyModifiedWeight <= 0.0f) {
        return 0.0f;
    }
    
    try {
        // Get last write time
        auto lastWriteTime = fs::last_write_time(filePath);
        auto now = fs::file_time_type::clock::now();
        
        // Calculate days since last modification
        auto duration = now - lastWriteTime;
        auto days = std::chrono::duration_cast<std::chrono::hours>(duration).count() / 24;
        
        // Score based on recency (linear falloff)
        if (days <= config_.recentTimeWindowDays) {
            float recencyFactor = 1.0f - (static_cast<float>(days) / static_cast<float>(config_.recentTimeWindowDays));
            return recencyFactor * config_.recentlyModifiedWeight;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error getting file modification time for " << filePath << ": " << e.what() << std::endl;
    }
    
    return 0.0f;
}

float FileScorer::scoreFileSize(const fs::path& filePath) {
    if (config_.fileSizeWeight <= 0.0f) {
        return 0.0f;
    }
    
    try {
        // Get file size
        size_t fileSize = fs::file_size(filePath);
        
        // Smaller files get higher scores (inverse relationship)
        if (fileSize <= config_.largeFileThreshold) {
            float sizeFactor = 1.0f - (static_cast<float>(fileSize) / static_cast<float>(config_.largeFileThreshold));
            return sizeFactor * config_.fileSizeWeight;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error getting file size for " << filePath << ": " << e.what() << std::endl;
    }
    
    return 0.0f;
}

float FileScorer::scoreCodeDensity(const fs::path& filePath) {
    if (config_.codeDensityWeight <= 0.0f) {
        return 0.0f;
    }
    
    // Only analyze source code files for density
    if (!isSourceCodeFile(filePath)) {
        return 0.0f;
    }
    
    try {
        // Use TreeSitter for better analysis if enabled
        if (config_.useTreeSitter) {
            return analyzeWithTreeSitter(filePath);
        }
        
        // Fallback to simple analysis
        std::ifstream file(filePath);
        if (!file) {
            return 0.0f;
        }
        
        std::string line;
        int totalLines = 0;
        int codeLines = 0;
        int commentLines = 0;
        int emptyLines = 0;
        int importLines = 0;
        int functionDefs = 0;
        int classDefs = 0;
        
        // Basic stack for tracking code blocks/scope depth
        int scopeDepth = 0;
        std::string extension = filePath.extension().string();
        bool inMultiLineComment = false;
        bool inString = false;
        char stringDelimiter = 0;
        
        // Regex patterns for detecting code structures based on language
        std::regex functionPattern;
        std::regex classPattern;
        std::regex importPattern;
        std::regex commentStartPattern;
        std::regex commentEndPattern;
        
        // Configure language-specific patterns
        if (extension == ".py") {
            functionPattern = std::regex("^\\s*def\\s+\\w+\\s*\\(.*\\)\\s*:");
            classPattern = std::regex("^\\s*class\\s+\\w+.*:");
            importPattern = std::regex("^\\s*(import|from)\\s+\\w+");
            commentStartPattern = std::regex("^\\s*\"\"\"");
            commentEndPattern = std::regex("\"\"\"\\s*$");
        } 
        else if (extension == ".js" || extension == ".ts" || extension == ".jsx" || extension == ".tsx") {
            functionPattern = std::regex("(function\\s+\\w+\\s*\\(|const\\s+\\w+\\s*=\\s*\\(|\\w+\\s*=\\s*\\(|\\w+\\s*\\(.*\\)\\s*\\{)");
            classPattern = std::regex("class\\s+\\w+");
            importPattern = std::regex("(import|require)");
            commentStartPattern = std::regex("/\\*");
            commentEndPattern = std::regex("\\*/");
        }
        else if (extension == ".cpp" || extension == ".cc" || extension == ".cxx" || extension == ".h" || extension == ".hpp") {
            functionPattern = std::regex("\\w+\\s+\\w+\\s*\\(.*\\)\\s*(const)?\\s*\\{?");
            classPattern = std::regex("(class|struct)\\s+\\w+");
            importPattern = std::regex("#include");
            commentStartPattern = std::regex("/\\*");
            commentEndPattern = std::regex("\\*/");
        }
        else if (extension == ".java") {
            functionPattern = std::regex("(public|private|protected)?\\s*(static)?\\s*\\w+\\s+\\w+\\s*\\(.*\\)\\s*\\{?");
            classPattern = std::regex("(public|private|protected)?\\s*(static)?\\s*class\\s+\\w+");
            importPattern = std::regex("import\\s+\\w+");
            commentStartPattern = std::regex("/\\*");
            commentEndPattern = std::regex("\\*/");
        }
        else if (extension == ".rb") {
            functionPattern = std::regex("def\\s+\\w+");
            classPattern = std::regex("class\\s+\\w+");
            importPattern = std::regex("(require|include)");
            commentStartPattern = std::regex("=begin");
            commentEndPattern = std::regex("=end");
        }
        
        while (std::getline(file, line)) {
            totalLines++;
            
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            
            // Track multiline comments
            if (!inMultiLineComment && std::regex_search(line, commentStartPattern)) {
                inMultiLineComment = true;
                commentLines++;
                continue;
            }
            
            if (inMultiLineComment) {
                commentLines++;
                if (std::regex_search(line, commentEndPattern)) {
                    inMultiLineComment = false;
                }
                continue;
            }
            
            // Handle empty lines
            if (line.empty()) {
                emptyLines++;
                continue;
            }
            
            // Handle single-line comments
            if (line.substr(0, 2) == "//" || line.substr(0, 1) == "#" || 
                line.substr(0, 2) == "--" || line.substr(0, 1) == ";") {
                commentLines++;
                continue;
            }
            
            // Count code structures
            if (std::regex_search(line, functionPattern)) {
                functionDefs++;
            }
            
            if (std::regex_search(line, classPattern)) {
                classDefs++;
            }
            
            if (std::regex_search(line, importPattern)) {
                importLines++;
            }
            
            // Basic scope tracking
            for (char c : line) {
                if (c == '{') scopeDepth++;
                if (c == '}') scopeDepth = std::max(0, scopeDepth - 1);
            }
            
            // It's a code line
            codeLines++;
        }
        
        // Calculate code density score (0.0 - 1.0)
        float density = static_cast<float>(codeLines) / static_cast<float>(totalLines > 0 ? totalLines : 1);
        
        // Apply additional analysis factors
        float structureBonus = 0.0f;
        
        // Bonus for files with function/class definitions (indicates important structural code)
        if (functionDefs > 0 || classDefs > 0) {
            structureBonus += std::min(0.2f, (functionDefs + classDefs * 2) * 0.02f);
        }
        
        // Penalty for files with no comments at all but lots of code
        float commentPenalty = 0.0f;
        if (commentLines == 0 && codeLines > 20) {
            commentPenalty = 0.1f;
        }
        
        // Bonus for balanced comment-to-code ratio
        float commentBonus = 0.0f;
        if (commentLines > 0) {
            float commentRatio = static_cast<float>(commentLines) / static_cast<float>(codeLines > 0 ? codeLines : 1);
            // Ideal range is 0.1-0.3 (10-30% comments)
            if (commentRatio >= 0.1f && commentRatio <= 0.3f) {
                commentBonus = 0.1f;
            }
        }
        
        // Import density can indicate a file that's more "glue" than substance
        float importPenalty = 0.0f;
        if (importLines > 5 && codeLines < importLines * 3) {
            importPenalty = 0.1f;
        }
        
        // Calculate final score
        float finalScore = (density * 0.6f) + structureBonus + commentBonus - commentPenalty - importPenalty;
        
        // Scale by the configured weight
        return std::min(1.0f, std::max(0.0f, finalScore)) * config_.codeDensityWeight;
    }
    catch (const std::exception& e) {
        std::cerr << "Error analyzing code density for " << filePath << ": " << e.what() << std::endl;
    }
    
    return 0.0f;
}

bool FileScorer::isSourceCodeFile(const fs::path& filePath) const {
    std::string extension = filePath.extension().string();
    return std::find(config_.sourceCodeExtensions.begin(), 
                     config_.sourceCodeExtensions.end(),
                     extension) != config_.sourceCodeExtensions.end();
}

bool FileScorer::isConfigFile(const fs::path& filePath) const {
    std::string extension = filePath.extension().string();
    return std::find(config_.configFileExtensions.begin(), 
                     config_.configFileExtensions.end(),
                     extension) != config_.configFileExtensions.end();
}

bool FileScorer::isDocumentationFile(const fs::path& filePath) const {
    std::string extension = filePath.extension().string();
    return std::find(config_.documentationExtensions.begin(), 
                     config_.documentationExtensions.end(),
                     extension) != config_.documentationExtensions.end();
}

bool FileScorer::isTestFile(const fs::path& filePath) const {
    std::string pathStr = filePath.string();
    return matchesAnyPattern(pathStr, config_.testFilePatterns);
}

bool FileScorer::isEntryPoint(const fs::path& filePath) const {
    std::string filename = filePath.filename().string();
    
    // Common entry point file names
    std::vector<std::string> entryPointPatterns = {
        "main.*", "index.*", "app.*", "server.*", "start.*", "init.*", "bootstrap.*"
    };
    
    return matchesAnyPattern(filename, entryPointPatterns);
}

bool FileScorer::matchesAnyPattern(const std::string& pathStr, const std::vector<std::string>& patterns) const {
    for (const auto& pattern : patterns) {
        // Convert glob pattern to regex
        std::string regexPattern = pattern;
        
        // Replace "." with literal "."
        size_t pos = 0;
        while ((pos = regexPattern.find(".", pos)) != std::string::npos) {
            regexPattern.replace(pos, 1, "\\.");
            pos += 2;
        }
        
        // Replace "*" with ".*"
        pos = 0;
        while ((pos = regexPattern.find("*", pos)) != std::string::npos) {
            regexPattern.replace(pos, 1, ".*");
            pos += 2;
        }
        
        std::regex re(regexPattern);
        if (std::regex_search(pathStr, re)) {
            return true;
        }
    }
    
    return false;
}

float FileScorer::calculateImportanceByLocation(const fs::path& filePath, const fs::path& repoRoot) const {
    fs::path relPath = fs::relative(filePath, repoRoot);
    int depth = std::distance(relPath.begin(), relPath.end());
    
    // Files closer to the root are generally more important
    return 1.0f / (depth + 1.0f);
}

std::unordered_map<std::string, std::vector<std::string>> FileScorer::buildDependencyGraph(const fs::path& repoRoot) {
    std::unordered_map<std::string, std::vector<std::string>> graph;
    
    // Map of file extensions to possible absolute paths (for dependency resolution)
    std::unordered_map<std::string, std::vector<fs::path>> filesByName;
    
    // First, index all files in the repository for faster lookups
    for (const auto& entry : fs::recursive_directory_iterator(repoRoot)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        
        std::string filename = entry.path().filename().string();
        std::string extension = entry.path().extension().string();
        filesByName[filename].push_back(entry.path());
        
        // Also store without extension for languages that import without extensions
        if (!extension.empty()) {
            std::string nameWithoutExt = filename.substr(0, filename.length() - extension.length());
            filesByName[nameWithoutExt].push_back(entry.path());
        }
    }
    
    // Now scan files for imports
    for (const auto& entry : fs::recursive_directory_iterator(repoRoot)) {
        if (!entry.is_regular_file() || !isSourceCodeFile(entry.path())) {
            continue;
        }
        
        try {
            std::string relPathStr = fs::relative(entry.path(), repoRoot).string();
            std::string extension = entry.path().extension().string();
            std::ifstream file(entry.path());
            std::string line;
            
            // Skip binary files
            if (!file.is_open() || !file) {
                continue;
            }
            
            // Initialize an empty vector for this file's imports
            if (graph.find(relPathStr) == graph.end()) {
                graph[relPathStr] = std::vector<std::string>();
            }
            
            // Track imports in multi-line scopes (e.g., import { a, b, c } from '...')
            bool inMultiLineImport = false;
            std::string multiLineImportSource;
            
            // Language-specific import regexes
            std::vector<std::regex> importRegexes;
            std::regex multiLineStartRegex;
            std::regex multiLineEndRegex;
            bool hasMultiLineImports = false;
            
            // Python-specific import handling
            bool isPython = (extension == ".py");
            std::set<std::string> pythonImports;
            
            // Set up language-specific regex patterns
            if (extension == ".js" || extension == ".ts" || extension == ".jsx" || extension == ".tsx") {
                // JavaScript/TypeScript imports
                importRegexes.push_back(std::regex("import\\s+.*?from\\s+['\"](.+?)['\"]"));
                importRegexes.push_back(std::regex("import\\s+['\"](.+?)['\"]"));
                importRegexes.push_back(std::regex("require\\s*\\(['\"](.+?)['\"]\\)"));
                
                // Handle multi-line imports like: import {
                //   Component1,
                //   Component2
                // } from 'source';
                multiLineStartRegex = std::regex("import\\s+\\{.*(?:from\\s+['\"](.+?)['\"])?$");
                multiLineEndRegex = std::regex(".*\\}\\s+from\\s+['\"](.+?)['\"]");
                hasMultiLineImports = true;
            }
            else if (extension == ".py") {
                // Python imports
                importRegexes.push_back(std::regex("from\\s+([\\w\\.]+)\\s+import"));
                importRegexes.push_back(std::regex("import\\s+([\\w\\.]+)"));
                
                // Handle multi-line imports with parentheses
                multiLineStartRegex = std::regex("from\\s+([\\w\\.]+)\\s+import\\s+\\(");
                multiLineEndRegex = std::regex("\\)");
                hasMultiLineImports = true;
            }
            else if (extension == ".java") {
                // Java imports
                importRegexes.push_back(std::regex("import\\s+([\\w\\.\\*]+);"));
            }
            else if (extension == ".cpp" || extension == ".cc" || extension == ".cxx" || extension == ".h" || extension == ".hpp") {
                // C/C++ includes
                importRegexes.push_back(std::regex("#include\\s+[<\"](.+?)[>\"]"));
            }
            else if (extension == ".rb") {
                // Ruby requires
                importRegexes.push_back(std::regex("require\\s+['\"](.+?)['\"]"));
                importRegexes.push_back(std::regex("require_relative\\s+['\"](.+?)['\"]"));
                importRegexes.push_back(std::regex("load\\s+['\"](.+?)['\"]"));
            }
            else if (extension == ".php") {
                // PHP includes
                importRegexes.push_back(std::regex("(require|include)(_once)?\\s+['\"](.+?)['\"]"));
                importRegexes.push_back(std::regex("use\\s+([\\w\\\\]+)"));
            }
            else if (extension == ".go") {
                // Go imports
                importRegexes.push_back(std::regex("import\\s+['\"](.+?)['\"]"));
                
                // Multi-line imports
                multiLineStartRegex = std::regex("import\\s+\\(");
                multiLineEndRegex = std::regex("\\)");
                hasMultiLineImports = true;
            }
            else if (extension == ".rs") {
                // Rust imports
                importRegexes.push_back(std::regex("use\\s+([\\w:]+)"));
                
                // Multi-line imports
                multiLineStartRegex = std::regex("use\\s+\\{");
                multiLineEndRegex = std::regex("\\};");
                hasMultiLineImports = true;
            }
            
            while (std::getline(file, line)) {
                // Handle multi-line imports if supported for this language
                if (hasMultiLineImports) {
                    if (!inMultiLineImport) {
                        std::smatch startMatch;
                        if (std::regex_search(line, startMatch, multiLineStartRegex)) {
                            inMultiLineImport = true;
                            if (startMatch.size() > 1 && !startMatch[1].str().empty()) {
                                multiLineImportSource = startMatch[1].str();
                            }
                            continue;
                        }
                    }
                    else {
                        // Check if multi-line import ends
                        std::smatch endMatch;
                        if (std::regex_search(line, endMatch, multiLineEndRegex)) {
                            inMultiLineImport = false;
                            
                            // Get import source from end match if available
                            if (endMatch.size() > 1 && !endMatch[1].str().empty()) {
                                multiLineImportSource = endMatch[1].str();
                            }
                            
                            // Add the import if we have a source
                            if (!multiLineImportSource.empty()) {
                                // Process the import based on language
                                std::string resolvedImport = resolveImportPath(multiLineImportSource, entry.path(), repoRoot, filesByName);
                                if (!resolvedImport.empty() && resolvedImport != relPathStr) {
                                    graph[relPathStr].push_back(resolvedImport);
                                }
                                multiLineImportSource.clear();
                            }
                            continue;
                        }
                        
                        // For some languages, extract imports from the multi-line block
                        if (isPython && !line.empty()) {
                            // Extract Python's inline imports within parentheses
                            std::string importName = line;
                            // Trim whitespace and commas
                            importName.erase(0, importName.find_first_not_of(" \t\r\n,"));
                            importName.erase(importName.find_last_not_of(" \t\r\n,") + 1);
                            
                            if (!importName.empty() && importName.find("#") != 0) { // Skip comments
                                pythonImports.insert(multiLineImportSource + "." + importName);
                            }
                        }
                        
                        continue;
                    }
                }
                
                // Skip comments
                if (line.find("//") == 0 || line.find("#") == 0) {
                    continue;
                }
                
                // Process regular imports using regex patterns
                for (const auto& regex : importRegexes) {
                    std::smatch matches;
                    std::string::const_iterator searchStart(line.cbegin());
                    
                    while (std::regex_search(searchStart, line.cend(), matches, regex)) {
                        if (matches.size() > 1) {
                            std::string importPath = matches[1].str();
                            
                            // Handle PHP's require/include which has the path in group 3
                            if (extension == ".php" && (matches[0].str().find("require") == 0 || 
                                                       matches[0].str().find("include") == 0)) {
                                importPath = matches[3].str();
                            }
                            
                            // For Python, collect imports for later resolution
                            if (isPython) {
                                pythonImports.insert(importPath);
                            } else {
                                // For other languages, resolve immediately
                                std::string resolvedImport = resolveImportPath(importPath, entry.path(), repoRoot, filesByName);
                                if (!resolvedImport.empty() && resolvedImport != relPathStr) {
                                    graph[relPathStr].push_back(resolvedImport);
                                }
                            }
                        }
                        
                        // Move to the next match
                        searchStart = matches.suffix().first;
                    }
                }
            }
            
            // Process collected Python imports
            if (isPython && !pythonImports.empty()) {
                for (const auto& importPath : pythonImports) {
                    // Convert dot notation to path
                    std::string pathWithSlashes = importPath;
                    std::replace(pathWithSlashes.begin(), pathWithSlashes.end(), '.', '/');
                    
                    // Try with .py extension
                    std::string resolvedImport = resolveImportPath(pathWithSlashes + ".py", entry.path(), repoRoot, filesByName);
                    if (resolvedImport.empty()) {
                        // Try as a directory with __init__.py
                        resolvedImport = resolveImportPath(pathWithSlashes + "/__init__.py", entry.path(), repoRoot, filesByName);
                    }
                    
                    if (!resolvedImport.empty() && resolvedImport != relPathStr) {
                        graph[relPathStr].push_back(resolvedImport);
                    }
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error analyzing dependencies in " << entry.path() << ": " << e.what() << std::endl;
        }
    }
    
    return graph;
}

// Helper function to resolve import paths to actual files
std::string FileScorer::resolveImportPath(const std::string& importPath, 
                                       const fs::path& sourceFile, 
                                       const fs::path& repoRoot,
                                       const std::unordered_map<std::string, std::vector<fs::path>>& filesByName) {
    // Handle absolute path (relative to repo root)
    if (importPath.front() == '/') {
        fs::path absolutePath = repoRoot / importPath.substr(1);
        if (fs::exists(absolutePath) && fs::is_regular_file(absolutePath)) {
            return fs::relative(absolutePath, repoRoot).string();
        }
    }
    
    // Handle relative path
    if (importPath.substr(0, 2) == "./" || importPath.substr(0, 3) == "../") {
        fs::path relativePath = sourceFile.parent_path() / importPath;
        
        // Normalize path
        try {
            relativePath = fs::canonical(relativePath);
            
            // Check if file exists
            if (fs::exists(relativePath) && fs::is_regular_file(relativePath)) {
                return fs::relative(relativePath, repoRoot).string();
            }
            
            // If path doesn't exist as-is, try adding common extensions
            for (const auto& ext : {".js", ".ts", ".jsx", ".tsx", ".py", ".java", ".rb", ".php", ".go", ".rs"}) {
                fs::path withExt = fs::path(relativePath.string() + ext);
                if (fs::exists(withExt) && fs::is_regular_file(withExt)) {
                    return fs::relative(withExt, repoRoot).string();
                }
            }
            
            // Handle index files in directories (common in JS/TS)
            for (const auto& indexFile : {"index.js", "index.ts", "index.jsx", "index.tsx", "__init__.py"}) {
                fs::path indexPath = relativePath / indexFile;
                if (fs::exists(indexPath) && fs::is_regular_file(indexPath)) {
                    return fs::relative(indexPath, repoRoot).string();
                }
            }
        }
        catch (const std::exception& e) {
            // Ignore errors in path resolution (e.g., invalid paths)
        }
    }
    
    // Try to match by filename for non-relative imports (e.g., modules)
    // Extract just the filename from the import path
    fs::path importPathObj(importPath);
    std::string filename = importPathObj.filename().string();
    
    if (filesByName.find(filename) != filesByName.end()) {
        const auto& possibleFiles = filesByName.at(filename);
        if (!possibleFiles.empty()) {
            // Return the first match (could be improved to find the most likely match)
            return fs::relative(possibleFiles.front(), repoRoot).string();
        }
    }
    
    // Could not resolve the import
    return "";
}

float FileScorer::calculateConnectivityScore(const fs::path& filePath, 
                                         const std::unordered_map<std::string, std::vector<std::string>>& depGraph) {
    std::string relPath = fs::relative(filePath, fs::current_path()).string();
    
    // Count incoming and outgoing connections
    int incomingConnections = 0;
    int outgoingConnections = 0;
    
    // Count outgoing connections (files this file imports)
    if (depGraph.find(relPath) != depGraph.end()) {
        outgoingConnections = depGraph.at(relPath).size();
    }
    
    // Count incoming connections (files that import this file)
    for (const auto& [file, imports] : depGraph) {
        if (file == relPath) {
            continue;
        }
        
        for (const auto& import : imports) {
            // Check if this import refers to the current file
            if (import == relPath || import.find(filePath.filename().string()) != std::string::npos) {
                incomingConnections++;
                break;
            }
        }
    }
    
    // Files with more connections are likely more important
    int totalConnections = incomingConnections + outgoingConnections;
    
    // Scale connectivity score based on total connections (diminishing returns)
    if (totalConnections == 0) {
        return 0.0f;
    }
    
    // Log scale to handle files with many connections
    return std::min(1.0f, std::log2f(totalConnections + 1) / 5.0f);
}

float FileScorer::analyzeWithTreeSitter(const fs::path& filePath) {
    try {
        // Read file content
        std::ifstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filePath << std::endl;
            return 0.0f;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        
        // Initialize TreeSitter parser
        TSParser* parser = ts_parser_new();
        if (!parser) {
            std::cerr << "Failed to create TreeSitter parser for " << filePath << std::endl;
            return 0.0f;
        }
        
        // Determine language based on file extension
        std::string extension = filePath.extension().string();
        TSLanguage* language = nullptr;
        
        if (extension == ".cpp" || extension == ".hpp" || extension == ".h" || extension == ".cc") {
            language = tree_sitter_cpp();
        } else if (extension == ".c") {
            language = tree_sitter_c();
        } else if (extension == ".py") {
            language = tree_sitter_python();
        } else if (extension == ".js" || extension == ".jsx") {
            language = tree_sitter_javascript();
        } else if (extension == ".ts" || extension == ".tsx") {
            language = tree_sitter_javascript(); // Use JavaScript parser as fallback for TypeScript
        } else {
            // Unsupported language, use simple analysis
            ts_parser_delete(parser);
            return analyzeFileContent(filePath, content);
        }
        
        // Set language in parser
        ts_parser_set_language(parser, language);
        
        // Parse file
        TSTree* tree = ts_parser_parse_string(parser, nullptr, content.c_str(), static_cast<uint32_t>(content.length()));
        if (!tree) {
            std::cerr << "Failed to parse file: " << filePath << std::endl;
            ts_parser_delete(parser);
            return analyzeFileContent(filePath, content);
        }
        
        // Get root node
        TSNode root = ts_tree_root_node(tree);
        
        // Analyze code complexity based on AST
        float complexity = 0.0f;
        
        // Count functions
        const char* function_query_str = "";
        if (extension == ".cpp" || extension == ".hpp" || extension == ".h" || extension == ".cc" || extension == ".c") {
            function_query_str = "(function_definition) @function";
        } else if (extension == ".py") {
            function_query_str = "(function_definition) @function";
        } else if (extension == ".js" || extension == ".jsx" || extension == ".ts" || extension == ".tsx") {
            function_query_str = "(function_declaration) @function";
        }
        
        uint32_t error_offset;
        TSQueryError error_type;
        TSQuery* function_query = ts_query_new(language, function_query_str, strlen(function_query_str), &error_offset, &error_type);
        
        if (function_query) {
            TSQueryCursor* function_cursor = ts_query_cursor_new();
            ts_query_cursor_exec(function_cursor, function_query, root);
            
            // Count functions
            int function_count = 0;
            TSQueryMatch match;
            while (ts_query_cursor_next_match(function_cursor, &match)) {
                function_count++;
            }
            
            // More functions generally means more complex code
            complexity += function_count * 0.1f;
            
            ts_query_cursor_delete(function_cursor);
            ts_query_delete(function_query);
        }
        
        // Count classes
        const char* class_query_str = "";
        if (extension == ".cpp" || extension == ".hpp" || extension == ".h" || extension == ".cc") {
            class_query_str = "(class_specifier) @class";
        } else if (extension == ".py") {
            class_query_str = "(class_definition) @class";
        } else if (extension == ".js" || extension == ".jsx" || extension == ".ts" || extension == ".tsx") {
            class_query_str = "(class_declaration) @class";
        }
        
        TSQuery* class_query = ts_query_new(language, class_query_str, strlen(class_query_str), &error_offset, &error_type);
        
        if (class_query) {
            TSQueryCursor* class_cursor = ts_query_cursor_new();
            ts_query_cursor_exec(class_cursor, class_query, root);
            
            // Count classes
            int class_count = 0;
            TSQueryMatch match;
            while (ts_query_cursor_next_match(class_cursor, &match)) {
                class_count++;
            }
            
            // Classes add to complexity
            complexity += class_count * 0.2f;
            
            ts_query_cursor_delete(class_cursor);
            ts_query_delete(class_query);
        }
        
        // Count conditional statements (if, else, switch, etc.)
        const char* conditional_query_str = "";
        if (extension == ".cpp" || extension == ".hpp" || extension == ".h" || extension == ".cc" || extension == ".c") {
            conditional_query_str = "((if_statement) @if (while_statement) @while (for_statement) @for (switch_statement) @switch)";
        } else if (extension == ".py") {
            conditional_query_str = "((if_statement) @if (while_statement) @while (for_statement) @for)";
        } else if (extension == ".js" || extension == ".jsx" || extension == ".ts" || extension == ".tsx") {
            conditional_query_str = "((if_statement) @if (while_statement) @while (for_statement) @for (switch_statement) @switch)";
        }
        
        TSQuery* conditional_query = ts_query_new(language, conditional_query_str, strlen(conditional_query_str), &error_offset, &error_type);
        
        if (conditional_query) {
            TSQueryCursor* conditional_cursor = ts_query_cursor_new();
            ts_query_cursor_exec(conditional_cursor, conditional_query, root);
            
            // Count conditional statements
            int conditional_count = 0;
            TSQueryMatch match;
            while (ts_query_cursor_next_match(conditional_cursor, &match)) {
                conditional_count++;
            }
            
            // Conditionals add to complexity
            complexity += conditional_count * 0.05f;
            
            ts_query_cursor_delete(conditional_cursor);
            ts_query_delete(conditional_query);
        }
        
        // Clean up
        ts_tree_delete(tree);
        ts_parser_delete(parser);
        
        // Normalize complexity score (0.0 - 1.0)
        return std::min(1.0f, complexity);
    } catch (const std::exception& e) {
        std::cerr << "Error analyzing file with tree-sitter: " << filePath << ": " << e.what() << std::endl;
        return analyzeFileContent(filePath);
    }
}

float FileScorer::analyzeFileContent(const fs::path& filePath, const std::string& content) {
    try {
        int totalLines = 0;
        int codeLines = 0;
        int commentLines = 0;
        int emptyLines = 0;
        int importLines = 0;
        int functionDefs = 0;
        int classDefs = 0;
        
        // Basic stack for tracking code blocks/scope depth
        int scopeDepth = 0;
        std::string extension = filePath.extension().string();
        bool inMultiLineComment = false;
        
        // Regex patterns for detecting code structures based on language
        std::regex functionPattern;
        std::regex classPattern;
        std::regex importPattern;
        std::regex commentStartPattern;
        std::regex commentEndPattern;
        
        // Configure language-specific patterns
        if (extension == ".py") {
            functionPattern = std::regex("^\\s*def\\s+\\w+\\s*\\(.*\\)\\s*:");
            classPattern = std::regex("^\\s*class\\s+\\w+.*:");
            importPattern = std::regex("^\\s*(import|from)\\s+\\w+");
            commentStartPattern = std::regex("^\\s*\"\"\"");
            commentEndPattern = std::regex("\"\"\"\\s*$");
        } 
        else if (extension == ".js" || extension == ".ts" || extension == ".jsx" || extension == ".tsx") {
            functionPattern = std::regex("(function\\s+\\w+\\s*\\(|const\\s+\\w+\\s*=\\s*\\(|\\w+\\s*=\\s*\\(|\\w+\\s*\\(.*\\)\\s*\\{)");
            classPattern = std::regex("class\\s+\\w+");
            importPattern = std::regex("(import|require)");
            commentStartPattern = std::regex("/\\*");
            commentEndPattern = std::regex("\\*/");
        }
        else if (extension == ".cpp" || extension == ".cc" || extension == ".cxx" || extension == ".h" || extension == ".hpp") {
            functionPattern = std::regex("\\w+\\s+\\w+\\s*\\(.*\\)\\s*(const)?\\s*\\{?");
            classPattern = std::regex("(class|struct)\\s+\\w+");
            importPattern = std::regex("#include");
            commentStartPattern = std::regex("/\\*");
            commentEndPattern = std::regex("\\*/");
        }
        else if (extension == ".java") {
            functionPattern = std::regex("(public|private|protected)?\\s*(static)?\\s*\\w+\\s+\\w+\\s*\\(.*\\)\\s*\\{?");
            classPattern = std::regex("(public|private|protected)?\\s*(static)?\\s*class\\s+\\w+");
            importPattern = std::regex("import\\s+\\w+");
            commentStartPattern = std::regex("/\\*");
            commentEndPattern = std::regex("\\*/");
        }
        else if (extension == ".rb") {
            functionPattern = std::regex("def\\s+\\w+");
            classPattern = std::regex("class\\s+\\w+");
            importPattern = std::regex("(require|include)");
            commentStartPattern = std::regex("=begin");
            commentEndPattern = std::regex("=end");
        }
        
        // Process file line by line
        std::istringstream stream(content);
        std::string line;
        
        while (std::getline(stream, line)) {
            totalLines++;
            
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            
            // Track multiline comments
            if (!inMultiLineComment && std::regex_search(line, commentStartPattern)) {
                inMultiLineComment = true;
                commentLines++;
                continue;
            }
            
            if (inMultiLineComment) {
                commentLines++;
                if (std::regex_search(line, commentEndPattern)) {
                    inMultiLineComment = false;
                }
                continue;
            }
            
            // Handle empty lines
            if (line.empty()) {
                emptyLines++;
                continue;
            }
            
            // Handle single-line comments
            if (line.substr(0, 2) == "//" || line.substr(0, 1) == "#" || 
                line.substr(0, 2) == "--" || line.substr(0, 1) == ";") {
                commentLines++;
                continue;
            }
            
            // Count code structures
            if (std::regex_search(line, functionPattern)) {
                functionDefs++;
            }
            
            if (std::regex_search(line, classPattern)) {
                classDefs++;
            }
            
            if (std::regex_search(line, importPattern)) {
                importLines++;
            }
            
            // Basic scope tracking
            for (char c : line) {
                if (c == '{') scopeDepth++;
                if (c == '}') scopeDepth = std::max(0, scopeDepth - 1);
            }
            
            // It's a code line
            codeLines++;
        }
        
        // Calculate code density score (0.0 - 1.0)
        float density = static_cast<float>(codeLines) / static_cast<float>(totalLines > 0 ? totalLines : 1);
        
        // Apply additional analysis factors
        float structureBonus = 0.0f;
        
        // Bonus for files with function/class definitions (indicates important structural code)
        if (functionDefs > 0 || classDefs > 0) {
            structureBonus += std::min(0.2f, (functionDefs + classDefs * 2) * 0.02f);
        }
        
        // Penalty for files with no comments at all but lots of code
        float commentPenalty = 0.0f;
        if (commentLines == 0 && codeLines > 20) {
            commentPenalty = 0.1f;
        }
        
        // Bonus for balanced comment-to-code ratio
        float commentBonus = 0.0f;
        if (commentLines > 0) {
            float commentRatio = static_cast<float>(commentLines) / static_cast<float>(codeLines > 0 ? codeLines : 1);
            // Ideal range is 0.1-0.3 (10-30% comments)
            if (commentRatio >= 0.1f && commentRatio <= 0.3f) {
                commentBonus = 0.1f;
            }
        }
        
        // Import density can indicate a file that's more "glue" than substance
        float importPenalty = 0.0f;
        if (importLines > 5 && codeLines < importLines * 3) {
            importPenalty = 0.1f;
        }
        
        // Calculate final score
        float finalScore = (density * 0.6f) + structureBonus + commentBonus - commentPenalty - importPenalty;
        
        // Normalize score
        return std::min(1.0f, std::max(0.0f, finalScore));
    }
    catch (const std::exception& e) {
        std::cerr << "Error analyzing file content: " << filePath << ": " << e.what() << std::endl;
        
        // Fall back to simple file type based scoring
        if (isSourceCodeFile(filePath)) {
            return 0.5f;
        }
        return 0.2f;
    }
}

float FileScorer::analyzeFileContent(const fs::path& filePath) {
    try {
        // Read file content
        std::ifstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filePath << std::endl;
            return 0.0f;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        
        return analyzeFileContent(filePath, content);
    }
    catch (const std::exception& e) {
        std::cerr << "Error reading file: " << filePath << ": " << e.what() << std::endl;
        return 0.0f;
    }
} 