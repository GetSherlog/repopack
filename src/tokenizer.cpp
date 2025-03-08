#include "tokenizer.hpp"
#include <stdexcept>

// In cpp-tiktoken, the header is in a subdirectory
#ifdef USE_TIKTOKEN
#include <tiktoken/encoding.h>
#endif

// Static initialization of encoding map
const std::unordered_map<std::string, TokenizerEncoding> Tokenizer::encodingMap_ = {
    {"cl100k", TokenizerEncoding::CL100K_BASE},
    {"cl100k_base", TokenizerEncoding::CL100K_BASE},
    {"p50k", TokenizerEncoding::P50K_BASE},
    {"p50k_base", TokenizerEncoding::P50K_BASE},
    {"p50k_edit", TokenizerEncoding::P50K_EDIT},
    {"r50k", TokenizerEncoding::R50K_BASE},
    {"r50k_base", TokenizerEncoding::R50K_BASE},
    {"gpt2", TokenizerEncoding::R50K_BASE},
    {"o200k", TokenizerEncoding::O200K_BASE},
    {"o200k_base", TokenizerEncoding::O200K_BASE}
};

Tokenizer::Tokenizer(TokenizerEncoding encoding) 
    : encodingType_(encoding) {
    initializeEncoding();
}

Tokenizer::~Tokenizer() = default;

void Tokenizer::initializeEncoding() {
#ifdef USE_TIKTOKEN
    // Convert our enum to the LanguageModel enum used by cpp-tiktoken
    LanguageModel model;
    
    switch (encodingType_) {
        case TokenizerEncoding::CL100K_BASE:
            encodingName_ = "cl100k_base";
            model = LanguageModel::CL100K_BASE;
            break;
        case TokenizerEncoding::P50K_BASE:
            encodingName_ = "p50k_base";
            model = LanguageModel::P50K_BASE;
            break;
        case TokenizerEncoding::P50K_EDIT:
            encodingName_ = "p50k_edit";
            model = LanguageModel::P50K_EDIT;
            break;
        case TokenizerEncoding::R50K_BASE:
            encodingName_ = "r50k_base";
            model = LanguageModel::R50K_BASE;
            break;
        case TokenizerEncoding::O200K_BASE:
            encodingName_ = "o200k_base";
            model = LanguageModel::O200K_BASE;
            break;
        default:
            throw std::runtime_error("Unsupported tokenizer encoding");
    }
    
    // Get the encoder from cpp-tiktoken
    encoding_ = GptEncoding::get_encoding(model);
    
    if (!encoding_) {
        throw std::runtime_error("Failed to initialize tokenizer with encoding " + encodingName_);
    }
#else
    encodingName_ = encodingToString(encodingType_);
#endif
}

size_t Tokenizer::countTokens(const std::string& text) const {
#ifdef USE_TIKTOKEN
    if (!encoding_) {
        throw std::runtime_error("Tokenizer not initialized");
    }
    
    // Encode the text into tokens using cpp-tiktoken
    auto tokens = encoding_->encode(text);
    
    // Return the number of tokens
    return tokens.size();
#else
    // Fallback implementation if tiktoken is not available
    // Use a more efficient approximation method
    
    // Cache these values to avoid repeated calls
    static const size_t AVG_CHARS_PER_TOKEN = 4; // Average characters per token
    
    // Quick estimate for empty or very short texts
    if (text.empty()) {
        return 0;
    }
    
    if (text.length() <= 4) {
        return 1;
    }
    
    // Count tokens based on word boundaries and special characters
    size_t tokenCount = 0;
    bool inWord = false;
    size_t currentWordLength = 0;
    
    for (const char c : text) {
        if (std::isspace(c) || c == '.' || c == ',' || c == '!' || c == '?' || 
            c == ':' || c == ';' || c == '(' || c == ')' || c == '[' || c == ']' || 
            c == '{' || c == '}' || c == '"' || c == '\'' || c == '`') {
            
            if (inWord) {
                // End of a word - add tokens based on word length
                tokenCount += (currentWordLength + AVG_CHARS_PER_TOKEN - 1) / AVG_CHARS_PER_TOKEN;
                inWord = false;
                currentWordLength = 0;
            }
            
            // Count separators and punctuation as potential tokens
            if (!std::isspace(c)) {
                tokenCount += 1;
            }
        } else {
            if (!inWord) {
                inWord = true;
            }
            currentWordLength++;
        }
    }
    
    // Handle the last word if the text doesn't end with a delimiter
    if (inWord) {
        tokenCount += (currentWordLength + AVG_CHARS_PER_TOKEN - 1) / AVG_CHARS_PER_TOKEN;
    }
    
    // Add a small overhead for special cases
    return std::max(tokenCount, static_cast<size_t>(1));
#endif
}

std::string Tokenizer::getEncodingName() const {
    return encodingName_;
}

std::vector<std::string> Tokenizer::getSupportedEncodings() {
    return {"cl100k_base", "p50k_base", "p50k_edit", "r50k_base", "o200k_base", "gpt2"};
}

TokenizerEncoding Tokenizer::encodingFromString(const std::string& encodingName) {
    auto it = encodingMap_.find(encodingName);
    if (it != encodingMap_.end()) {
        return it->second;
    }
    throw std::runtime_error("Unsupported encoding name: " + encodingName);
}

std::string Tokenizer::encodingToString(TokenizerEncoding encoding) {
    switch (encoding) {
        case TokenizerEncoding::CL100K_BASE:
            return "cl100k_base";
        case TokenizerEncoding::P50K_BASE:
            return "p50k_base";
        case TokenizerEncoding::P50K_EDIT:
            return "p50k_edit";
        case TokenizerEncoding::R50K_BASE:
            return "r50k_base";
        case TokenizerEncoding::O200K_BASE:
            return "o200k_base";
        default:
            throw std::runtime_error("Unknown encoding enum value");
    }
} 