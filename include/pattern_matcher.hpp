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
    
    // Load patterns from a .gitignore file
    void loadGitignore(const fs::path& gitignorePath);
    
    // Check if a file matches any ignore pattern
    bool isIgnored(const fs::path& filePath) const;
    
private:
    std::vector<std::string> ignorePatterns_;
    std::vector<std::regex> ignoreRegexes_;
    
    // Helper methods
    std::regex patternToRegex(const std::string& pattern) const;
};