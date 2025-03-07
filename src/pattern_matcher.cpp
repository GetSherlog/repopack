#include "pattern_matcher.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <sstream>

PatternMatcher::PatternMatcher() {
    // Add some default ignore patterns
    addIgnorePattern(".git/**");
    addIgnorePattern("node_modules/**");
    addIgnorePattern("*.o");
    addIgnorePattern("*.obj");
    addIgnorePattern("*.exe");
    addIgnorePattern("*.dll");
    addIgnorePattern("*.lib");
    addIgnorePattern("*.a");
    addIgnorePattern("*.so");
    addIgnorePattern("*.pyc");
    addIgnorePattern("__pycache__/**");
    addIgnorePattern(".DS_Store");
}

PatternMatcher::PatternMatcher(const std::vector<std::string>& ignorePatterns) 
    : PatternMatcher() {
    
    for (const auto& pattern : ignorePatterns) {
        addIgnorePattern(pattern);
    }
}

void PatternMatcher::addIgnorePattern(const std::string& pattern) {
    ignorePatterns_.push_back(pattern);
    ignoreRegexes_.push_back(patternToRegex(pattern));
}

void PatternMatcher::addIncludePattern(const std::string& pattern) {
    includePatterns_.push_back(pattern);
    includeRegexes_.push_back(patternToRegex(pattern));
}

void PatternMatcher::setIncludePatterns(const std::string& patternsStr) {
    // Clear existing include patterns
    includePatterns_.clear();
    includeRegexes_.clear();
    
    // Split comma-separated string and add each pattern
    for (const auto& pattern : splitPatternString(patternsStr)) {
        addIncludePattern(pattern);
    }
}

void PatternMatcher::setExcludePatterns(const std::string& patternsStr) {
    // Split comma-separated string and add each pattern as ignore pattern
    for (const auto& pattern : splitPatternString(patternsStr)) {
        addIgnorePattern(pattern);
    }
}

std::vector<std::string> PatternMatcher::splitPatternString(const std::string& patternsStr) const {
    std::vector<std::string> patterns;
    std::stringstream ss(patternsStr);
    std::string pattern;
    
    while (std::getline(ss, pattern, ',')) {
        // Trim whitespace
        pattern.erase(pattern.begin(), std::find_if(pattern.begin(), pattern.end(), 
            [](unsigned char ch) { return !std::isspace(ch); }));
        pattern.erase(std::find_if(pattern.rbegin(), pattern.rend(), 
            [](unsigned char ch) { return !std::isspace(ch); }).base(), pattern.end());
        
        if (!pattern.empty()) {
            patterns.push_back(pattern);
        }
    }
    
    return patterns;
}

void PatternMatcher::loadGitignore(const fs::path& gitignorePath) {
    std::ifstream file(gitignorePath);
    if (!file) {
        std::cerr << "Warning: Failed to open .gitignore file: " << gitignorePath << std::endl;
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Trim whitespace
        line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        line.erase(std::find_if(line.rbegin(), line.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), line.end());
        
        // Skip empty lines after trimming
        if (line.empty()) {
            continue;
        }
        
        // Add pattern
        addIgnorePattern(line);
    }
}

bool PatternMatcher::shouldProcess(const fs::path& filePath) const {
    // First check if the file is ignored
    if (isIgnored(filePath)) {
        return false;
    }
    
    // If there are include patterns, file must match at least one
    if (!includePatterns_.empty()) {
        return isIncluded(filePath);
    }
    
    // If no include patterns, process all non-ignored files
    return true;
}

bool PatternMatcher::isIgnored(const fs::path& filePath) const {
    // Convert file path to string
    const auto pathStr = filePath.string();
    
    // Check if file path matches any ignore pattern
    for (size_t i = 0; i < ignorePatterns_.size(); ++i) {
        const auto& pattern = ignorePatterns_[i];
        const auto& regex = ignoreRegexes_[i];

        // Special case for simple extensions like "*.o"
        if (pattern.size() >= 2 && pattern[0] == '*' && pattern[1] == '.') {
            const std::string extension = pattern.substr(1);  // Get ".o" for example
            if (pathStr.size() >= extension.size() && 
                pathStr.substr(pathStr.size() - extension.size()) == extension) {
                return true;
            }
        }
        
        // Try regex match
        if (std::regex_match(pathStr, regex)) {
            return true;
        }
        
        // Special case for wildcard patterns
        if (pattern[0] == '*' && pattern[1] == '.' && pattern.find('/') == std::string::npos) {
            // Check if this is just a file extension pattern without path components
            // Only match the filename part of the path
            fs::path p(pathStr);
            std::string filename = p.filename().string();
            if (std::regex_match(filename, regex)) {
                return true;
            }
        }
    }
    
    return false;
}

bool PatternMatcher::isIncluded(const fs::path& filePath) const {
    // If no include patterns, everything is included
    if (includePatterns_.empty()) {
        return true;
    }
    
    // Convert file path to string
    const auto pathStr = filePath.string();
    
    // Check if file path matches any include pattern
    for (size_t i = 0; i < includePatterns_.size(); ++i) {
        const auto& pattern = includePatterns_[i];
        const auto& regex = includeRegexes_[i];

        // Special case for simple extensions like "*.cpp"
        if (pattern.size() >= 2 && pattern[0] == '*' && pattern[1] == '.') {
            const std::string extension = pattern.substr(1);  // Get ".cpp" for example
            if (pathStr.size() >= extension.size() && 
                pathStr.substr(pathStr.size() - extension.size()) == extension) {
                return true;
            }
        }
        
        // Try regex match
        if (std::regex_match(pathStr, regex)) {
            return true;
        }
        
        // Special case for wildcard patterns
        if (pattern[0] == '*' && pattern[1] == '.' && pattern.find('/') == std::string::npos) {
            // Check if this is just a file extension pattern without path components
            // Only match the filename part of the path
            fs::path p(pathStr);
            std::string filename = p.filename().string();
            if (std::regex_match(filename, regex)) {
                return true;
            }
        }
    }
    
    return false;
}

std::regex PatternMatcher::patternToRegex(const std::string& pattern) const {
    // Convert pattern to regex
    std::string regexStr = "";
    
    // Check if pattern needs to match from start
    bool matchFromStart = !(pattern.find("/") != 0 && pattern.find("**/") != 0);
    if (matchFromStart) {
        regexStr += "^";
    }
    
    for (size_t i = 0; i < pattern.size(); ++i) {
        const char c = pattern[i];
        
        if (c == '*') {
            if (i + 1 < pattern.size() && pattern[i + 1] == '*') {
                // ** matches any path (including directories)
                if (i + 2 < pattern.size() && pattern[i + 2] == '/') {
                    // **/ matches any directory depth
                    regexStr += "(?:.*?/)?";
                    i += 2; // Skip next '**/'
                } else {
                    // Just ** matches anything
                    regexStr += ".*";
                    i++; // Skip next *
                }
            } else {
                // * matches any character except directory separator
                regexStr += "[^/]*";
            }
        } else if (c == '?') {
            // ? matches any single character except directory separator
            regexStr += "[^/]";
        } else if (c == '.') {
            // Escape dot
            regexStr += "\\.";
        } else if (c == '/') {
            // / matches directory separator
            regexStr += "/";
        } else if (c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' || 
                  c == '+' || c == '^' || c == '$' || c == '|') {
            // Escape special regex characters
            regexStr += '\\';
            regexStr += c;
        } else {
            // Other characters match literally
            regexStr += c;
        }
    }
    
    // Add end of string if pattern ends with not a directory
    if (pattern.empty() || pattern.back() != '/') {
        regexStr += "$";
    }
    
    return std::regex(regexStr);
}