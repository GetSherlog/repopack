#pragma once

#include <string>
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>

// Forward declaration to hide tiktoken implementation details
namespace tiktoken_cpp {
    class Encoding;
}

// Supported tokenizer encodings
enum class TokenizerEncoding {
    CL100K_BASE,  // ChatGPT models, text-embedding-ada-002
    P50K_BASE,    // Code models, text-davinci-002, text-davinci-003
    P50K_EDIT,    // Edit models like text-davinci-edit-001, code-davinci-edit-001
    R50K_BASE,    // GPT-3 models like davinci
    O200K_BASE    // GPT-4o models
};

class Tokenizer {
public:
    Tokenizer(TokenizerEncoding encoding = TokenizerEncoding::CL100K_BASE);
    ~Tokenizer();

    // Count tokens in a string
    size_t countTokens(const std::string& text) const;
    
    // Get the encoding name as a string
    std::string getEncodingName() const;
    
    // Get all supported encoding names
    static std::vector<std::string> getSupportedEncodings();
    
    // Convert string to TokenizerEncoding enum
    static TokenizerEncoding encodingFromString(const std::string& encodingName);
    
    // Convert TokenizerEncoding enum to string
    static std::string encodingToString(TokenizerEncoding encoding);

private:
    // Pointer to the tiktoken Encoding implementation
    std::unique_ptr<tiktoken_cpp::Encoding> encoding_;
    TokenizerEncoding encodingType_;
    
    // Cache string representation of encoding type
    std::string encodingName_;
    
    // Static mapping of encoding names to enum values
    static const std::unordered_map<std::string, TokenizerEncoding> encodingMap_;
    
    // Initialize the tiktoken Encoding based on the encoding type
    void initializeEncoding();
}; 