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

// For ONNX Runtime
#ifdef USE_ONNX_RUNTIME
#include <onnxruntime_cxx_api.h>
#else
// Forward declaration for ONNX Runtime headers when not using the actual headers
namespace Ort {
    class Session;
    class Env;
    class MemoryInfo;
    struct Value;
}
#endif

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
    struct MLImpl;
    std::unique_ptr<MLImpl> impl_;
    
    // Static cache for entity extraction results
    static std::unordered_map<std::string, std::vector<NamedEntity>> entityCache_;
    static std::mutex cacheMutex_;
    
    // Helper methods for ML-based NER
    bool initializeModel() const;
    bool loadVocabulary(const std::string& vocabPath) const;
    std::vector<int32_t> tokenize(const std::string& text) const;
    std::vector<NamedEntity> runInference(const std::string& content, const fs::path& filePath) const;
    std::vector<std::pair<std::string, EntityType>> extractEntitiesFromLabels(
        const std::vector<std::string>& tokens,
        const std::vector<std::string>& labels
    ) const;
    EntityType mapEntityTypeFromModel(const std::string& mlEntityType) const;
    
    std::string getModelPath() const;
};

// Hybrid NER combining multiple approaches
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