#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <filesystem>
#include <chrono>
#include <mutex>
#include <optional>
#include <unordered_set>
#include <unordered_map>
#include <regex>
#include "repomix.hpp"  // For SummarizationOptions
#include "tree_sitter_types.hpp"  // For TreeSitter types

// Forward declaration for ONNX Runtime headers
namespace Ort {
    class Session;
    class Env;
    class MemoryInfo;
    struct Value;
}

namespace fs = std::filesystem;

// Forward declaration
class FileProcessor;

// Base class for Named Entity Recognition in code
class CodeNER {
public:
    // Named Entity structure from FileProcessor
    using EntityType = FileProcessor::NamedEntity::EntityType;
    using NamedEntity = FileProcessor::NamedEntity;
    
    virtual ~CodeNER() = default;
    
    // Extract entities from content
    virtual std::vector<NamedEntity> extractEntities(
        const std::string& content, 
        const fs::path& filePath
    ) const = 0;
    
    // Factory method to create the appropriate NER instance based on options
    static std::unique_ptr<CodeNER> create(const SummarizationOptions& options);
};

// Simple regex-based NER (original implementation)
class RegexNER : public CodeNER {
public:
    explicit RegexNER(const SummarizationOptions& options);
    
    std::vector<NamedEntity> extractEntities(
        const std::string& content, 
        const fs::path& filePath
    ) const override;
    
private:
    const SummarizationOptions& options_;
    
    // Specialized extraction methods
    std::vector<NamedEntity> extractClassNames(const std::string& content, const fs::path& filePath) const;
    std::vector<NamedEntity> extractFunctionNames(const std::string& content, const fs::path& filePath) const;
    std::vector<NamedEntity> extractVariableNames(const std::string& content, const fs::path& filePath) const;
    std::vector<NamedEntity> extractEnumValues(const std::string& content, const fs::path& filePath) const;
    std::vector<NamedEntity> extractImports(const std::string& content, const fs::path& filePath) const;
};

// Tree-sitter based NER for more accurate parsing
class TreeSitterNER : public CodeNER {
public:
    explicit TreeSitterNER(const SummarizationOptions& options);
    ~TreeSitterNER() override;
    
    std::vector<NamedEntity> extractEntities(
        const std::string& content, 
        const fs::path& filePath
    ) const override;
    
private:
    const SummarizationOptions& options_;
    
    // Tree-sitter specific implementation
    struct TreeSitterImpl;
    std::unique_ptr<TreeSitterImpl> impl_;
    
    // Helper methods for tree-sitter parsing
    bool initializeParser(const fs::path& filePath) const;
    std::string getLanguage(const fs::path& filePath) const;
};

// Machine Learning based NER using CodeBERT with ONNX Runtime
class MLNER : public CodeNER {
public:
    explicit MLNER(const SummarizationOptions& options);
    ~MLNER() override;
    
    std::vector<NamedEntity> extractEntities(
        const std::string& content, 
        const fs::path& filePath
    ) const override;
    
private:
    const SummarizationOptions& options_;
    
    // CodeBERT model details
    struct TokenizerConfig {
        std::string vocabFile;
        std::unordered_map<std::string, int32_t> vocabMap;
        std::vector<std::string> idToToken;
        int32_t unkTokenId = 100;
        int32_t padTokenId = 0;
        int32_t clsTokenId = 101;
        int32_t sepTokenId = 102;
        int32_t maxSeqLength = 512;
    };
    
    // ONNX Runtime implementation details
    struct MLImpl {
        // ONNX Runtime session and environment
        std::unique_ptr<Ort::Env> env;
        std::unique_ptr<Ort::Session> session;
        std::unique_ptr<Ort::MemoryInfo> memoryInfo;
        bool modelLoaded = false;
        TokenizerConfig tokenizerConfig;
        
        // Entity label map
        std::unordered_map<int, std::string> labelMap = {
            {0, "O"},        // Outside any entity
            {1, "B-CLASS"},  // Beginning of class
            {2, "I-CLASS"},  // Inside class
            {3, "B-FUNC"},   // Beginning of function
            {4, "I-FUNC"},   // Inside function
            {5, "B-VAR"},    // Beginning of variable
            {6, "I-VAR"},    // Inside variable
            {7, "B-ENUM"},   // Beginning of enum
            {8, "I-ENUM"},   // Inside enum
            {9, "B-IMP"},    // Beginning of import
            {10, "I-IMP"}    // Inside import
        };
    };
    
    std::unique_ptr<MLImpl> impl_;
    
    // ML model cache to avoid reloading
    static std::unordered_map<std::string, std::vector<NamedEntity>> entityCache_;
    static std::mutex cacheMutex_;
    
    // Helper methods for CodeBERT
    bool initializeModel() const;
    bool loadVocabulary(const std::string& vocabPath) const;
    std::vector<int32_t> tokenize(const std::string& text) const;
    std::vector<NamedEntity> runInference(const std::string& content, const fs::path& filePath) const;
    std::vector<std::pair<std::string, EntityType>> extractEntitiesFromLabels(
        const std::vector<std::string>& tokens,
        const std::vector<int>& labels) const;
    
    // Convert ML output to NamedEntity objects
    EntityType mapEntityTypeFromModel(const std::string& mlEntityType) const;
    
    // Load the ONNX model file
    std::string getModelPath() const;
};

// Hybrid NER that chooses the best approach based on file characteristics
class HybridNER : public CodeNER {
public:
    explicit HybridNER(const SummarizationOptions& options);
    
    std::vector<NamedEntity> extractEntities(
        const std::string& content, 
        const fs::path& filePath
    ) const override;
    
private:
    const SummarizationOptions& options_;
    std::unique_ptr<RegexNER> regexNER_;
    std::unique_ptr<TreeSitterNER> treeSitterNER_;
    std::unique_ptr<MLNER> mlNER_;
    
    // Method to determine which NER approach to use
    CodeNER* chooseNERMethod(const std::string& content, const fs::path& filePath) const;
}; 