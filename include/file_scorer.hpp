#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <nlohmann/json.hpp>
#include <functional>
#include "pattern_matcher.hpp"

namespace fs = std::filesystem;

// Forward declarations
class FileProcessor;
struct SummarizationOptions;

// Configuration for the file scoring system
struct FileScoringConfig {
    // Project structure weights
    float rootFilesWeight = 0.9f;            // Root level files importance
    float topLevelDirsWeight = 0.8f;         // Top level directories importance
    float entryPointsWeight = 0.8f;          // Entry point files importance
    float dependencyGraphWeight = 0.7f;      // Files connected in dependency graph

    // File type weights
    float sourceCodeWeight = 0.8f;           // Source code files importance
    float configFilesWeight = 0.7f;          // Configuration files importance
    float documentationWeight = 0.6f;        // Documentation files importance
    float testFilesWeight = 0.5f;            // Test files importance

    // Recency weights
    float recentlyModifiedWeight = 0.7f;     // Recently modified files importance
    int recentTimeWindowDays = 7;            // What constitutes "recent" in days

    // File size weights (inverse - smaller files score higher)
    float fileSizeWeight = 0.4f;             // How much file size affects score (inversely)
    size_t largeFileThreshold = 1000000;     // 1MB - files larger than this are considered large

    // Code density
    float codeDensityWeight = 0.5f;          // Code density importance (higher density = higher score)

    // Minimum score threshold for inclusion (0.0 - 1.0)
    float inclusionThreshold = 0.3f;         // Minimum score needed to include a file

    // Special file patterns that get boosted scores
    std::vector<std::string> importantFilePatterns = {
        "README.md", "package.json", "requirements.txt", "setup.py", "Makefile",
        "CMakeLists.txt", ".gitignore", "Dockerfile", "docker-compose.yml",
        ".eslintrc.*", "tsconfig.json", "*.config.js", "main.*", "index.*", "app.*"
    };

    // Special directory patterns that get boosted scores
    std::vector<std::string> importantDirPatterns = {
        "src/", "lib/", "app/", "source/", "include/", "core/"
    };

    // Special file extensions to categorize file types
    std::vector<std::string> sourceCodeExtensions = {
        ".c", ".cpp", ".cc", ".cxx", ".h", ".hpp", ".js", ".ts", ".jsx", ".tsx", 
        ".py", ".java", ".go", ".rs", ".rb", ".php", ".swift"
    };

    std::vector<std::string> configFileExtensions = {
        ".json", ".yaml", ".yml", ".toml", ".ini", ".cfg", ".conf"
    };

    std::vector<std::string> documentationExtensions = {
        ".md", ".txt", ".rst", ".adoc", ".pdf", ".doc", ".docx"
    };

    std::vector<std::string> testFilePatterns = {
        "test_*", "*_test.*", "*_spec.*", "*Test.*", "*Spec.*", "*/test/*", "*/tests/*"
    };

    // TreeSitter usage for improved parsing
    bool useTreeSitter = true;
};

class FileScorer {
public:
    // Struct to hold scoring results
    struct ScoredFile {
        fs::path path;
        float score;
        std::unordered_map<std::string, float> componentScores; // Individual score components
        bool included;  // Whether the file is included based on score threshold
    };

    // Constructor
    explicit FileScorer(const FileScoringConfig& config = FileScoringConfig());
    
    // Set the configuration
    void setConfig(const FileScoringConfig& config);
    
    // Score all files in a repository
    std::vector<ScoredFile> scoreRepository(const fs::path& repoPath);
    
    // Get score for a single file
    ScoredFile scoreFile(const fs::path& filePath, const fs::path& repoRoot);
    
    // Filter files based on scores and threshold
    std::vector<fs::path> getSelectedFiles(const std::vector<ScoredFile>& scoredFiles);
    
    // Get detailed scoring report as JSON string
    std::string getScoringReport(const std::vector<ScoredFile>& scoredFiles) const;

    // Get the current configuration
    const FileScoringConfig& getConfig() const;

private:
    FileScoringConfig config_;
    std::unique_ptr<PatternMatcher> patternMatcher_;
    
    // Scoring components
    float scoreProjectStructure(const fs::path& filePath, const fs::path& repoRoot);
    float scoreFileType(const fs::path& filePath);
    float scoreRecency(const fs::path& filePath);
    float scoreFileSize(const fs::path& filePath);
    float scoreCodeDensity(const fs::path& filePath);
    
    // Helper methods
    bool isSourceCodeFile(const fs::path& filePath) const;
    bool isConfigFile(const fs::path& filePath) const;
    bool isDocumentationFile(const fs::path& filePath) const;
    bool isTestFile(const fs::path& filePath) const;
    bool isEntryPoint(const fs::path& filePath) const;
    bool matchesAnyPattern(const std::string& pathStr, const std::vector<std::string>& patterns) const;
    float calculateImportanceByLocation(const fs::path& filePath, const fs::path& repoRoot) const;
    
    // Dependency graph analysis
    std::unordered_map<std::string, std::vector<std::string>> buildDependencyGraph(const fs::path& repoRoot);
    float calculateConnectivityScore(const fs::path& filePath, 
                                    const std::unordered_map<std::string, std::vector<std::string>>& depGraph);
    
    // Import path resolution
    std::string resolveImportPath(const std::string& importPath, 
                               const fs::path& sourceFile, 
                               const fs::path& repoRoot,
                               const std::unordered_map<std::string, std::vector<fs::path>>& filesByName);
    
    // TreeSitter integration helper
    float analyzeWithTreeSitter(const fs::path& filePath);
    
    // Fallback methods for file analysis without tree-sitter
    float analyzeFileContent(const fs::path& filePath, const std::string& content);
    float analyzeFileContent(const fs::path& filePath);
}; 