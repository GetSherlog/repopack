#include "tokenizer.hpp"
#include <tiktoken_cpp.hpp>
#include <stdexcept>

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
    switch (encodingType_) {
        case TokenizerEncoding::CL100K_BASE:
            encoding_ = std::make_unique<tiktoken_cpp::Encoding>(tiktoken_cpp::cl100k_base());
            encodingName_ = "cl100k_base";
            break;
        case TokenizerEncoding::P50K_BASE:
            encoding_ = std::make_unique<tiktoken_cpp::Encoding>(tiktoken_cpp::p50k_base());
            encodingName_ = "p50k_base";
            break;
        case TokenizerEncoding::P50K_EDIT:
            encoding_ = std::make_unique<tiktoken_cpp::Encoding>(tiktoken_cpp::p50k_edit());
            encodingName_ = "p50k_edit";
            break;
        case TokenizerEncoding::R50K_BASE:
            encoding_ = std::make_unique<tiktoken_cpp::Encoding>(tiktoken_cpp::r50k_base());
            encodingName_ = "r50k_base";
            break;
        case TokenizerEncoding::O200K_BASE:
            encoding_ = std::make_unique<tiktoken_cpp::Encoding>(tiktoken_cpp::o200k_base());
            encodingName_ = "o200k_base";
            break;
        default:
            throw std::runtime_error("Unsupported tokenizer encoding");
    }
}

size_t Tokenizer::countTokens(const std::string& text) const {
    if (!encoding_) {
        throw std::runtime_error("Tokenizer not initialized");
    }
    
    // Encode the text into tokens using tiktoken
    auto tokens = encoding_->encode(text);
    
    // Return the number of tokens
    return tokens.size();
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