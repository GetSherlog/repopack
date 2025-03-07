#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

class PatternMatcher {
public:
    // Default constructor with default ignore patterns
    PatternMatcher();
    
    // Constructor with custom ignore patterns
    PatternMatcher(const std::vector<std::string>& ignorePatterns);
    
    // Add a new ignore pattern
    void addIgnorePattern(const std::string& pattern);
    
    // Add include patterns (files that match these will be processed)
    void addIncludePattern(const std::string& pattern);
    
    // Set include patterns from a comma-separated string (e.g., "*.cpp,*.hpp")
    void setIncludePatterns(const std::string& patternsStr);
    
    // Set exclude patterns from a comma-separated string (e.g., "*.txt,*.md")
    void setExcludePatterns(const std::string& patternsStr);
    
    // Load patterns from a .gitignore file
    void loadGitignore(const fs::path& gitignorePath);
    
    // Check if a file should be processed (matches include patterns and doesn't match ignore patterns)
    bool shouldProcess(const fs::path& filePath) const;
    
    // Check if a file matches any ignore pattern
    bool isIgnored(const fs::path& filePath) const;
    
    // Check if a file matches any include pattern
    bool isIncluded(const fs::path& filePath) const;
    
    // Check if any include patterns have been specified
    bool hasIncludePatterns() const { return !includePatterns_.empty(); }
    
private:
    std::vector<std::string> ignorePatterns_;
    std::vector<std::regex> ignoreRegexes_;
    std::vector<std::string> includePatterns_;
    std::vector<std::regex> includeRegexes_;
    
    // Helper methods
    std::regex patternToRegex(const std::string& pattern) const;
    std::vector<std::string> splitPatternString(const std::string& patternsStr) const;
};