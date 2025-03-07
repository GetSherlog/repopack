#include <drogon/drogon.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <memory>
#include <sys/stat.h>
#include <filesystem>
#include <curl/curl.h>
#include <regex>
#include "repomix.hpp"
#include <drogon/utils/Utilities.h>
#include <chrono>
#include <cstdlib>
#include <cstring>

using json = nlohmann::json;
namespace fs = std::filesystem;

// Global settings
std::string SHARED_DIRECTORY = "/app/shared";

// Callback function for curl to write response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t realsize = size * nmemb;
    userp->append((char*)contents, realsize);
    return realsize;
}

// Temporary file handling
std::string createTempDir() {
    auto timestamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    std::string tempDir = "/tmp/repomix_" + timestamp;
    
    // Create directory
    if (mkdir(tempDir.c_str(), 0755) != 0) {
        throw std::runtime_error("Failed to create temp directory: " + tempDir);
    }
    
    return tempDir;
}

// Utility function to check if a string is base64 encoded
// If it is, optionally returns the decoded content via outDecoded
bool isBase64Encoded(const std::string& str, std::string* outDecoded = nullptr) {
    // If empty or too short, it's not base64
    if (str.empty() || str.size() < 4) {
        std::cout << "Base64 check failed: String too short (" << str.size() << " chars)" << std::endl;
        return false;
    }
    
    // Quick check of characters - base64 should only contain A-Z, a-z, 0-9, +, /, and = for padding
    const std::string validChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
    
    // Check the first 100 characters to see if they match base64 pattern
    // This helps avoid expensive decoding of large outputs that aren't base64
    int sampleSize = std::min(100, static_cast<int>(str.size()));
    int validCount = 0;
    
    for (int i = 0; i < sampleSize; i++) {
        if (validChars.find(str[i]) != std::string::npos) {
            validCount++;
        }
    }
    
    // If less than 95% of characters in the sample are valid base64 chars, it's not base64
    double validRatio = static_cast<double>(validCount) / sampleSize;
    if (validRatio < 0.95) {
        std::cout << "Base64 check failed: Invalid character ratio " << validRatio << std::endl;
        return false;
    }
    
    // Try decoding with Drogon
    try {
        std::string decoded = drogon::utils::base64Decode(str);
        
        if (decoded.empty()) {
            std::cout << "Base64 decoding resulted in empty string" << std::endl;
            return false;
        }
        
        // Check if decoded content looks like valid text or binary data
        bool seemsValid = false;
        double textRatio = 0.0;
        int textChars = 0;
        
        // Count how many chars in decoded string look like normal text
        for (char c : decoded) {
            if ((c >= 32 && c <= 126) || c == '\n' || c == '\r' || c == '\t') {
                textChars++;
            }
        }
        
        textRatio = static_cast<double>(textChars) / decoded.size();
        std::cout << "Decoded content text ratio: " << textRatio << std::endl;
        
        // Be more conservative: only consider it base64 if >90% looks like text
        // (previously was 75% or 30% for larger content)
        if (textRatio > 0.9) {
            seemsValid = true;
        }
        
        if (!seemsValid) {
            std::cout << "Decoded content doesn't appear to be valid text (text ratio: " << textRatio << ")" << std::endl;
            return false;
        }
        
        // If we reach here, it seems like valid Base64
        std::cout << "Successful Base64 decoding with content length " << decoded.size() << " bytes" << std::endl;
        
        // If caller wants the decoded content, provide it
        if (outDecoded) {
            *outDecoded = std::move(decoded);
        }
        return true;
    } catch (const std::exception& e) {
        std::cout << "Base64 decoding failed with exception: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cout << "Base64 decoding failed with unknown exception" << std::endl;
        return false;
    }
}

// Use Drogon's utility for base64 decoding
std::string decodeBase64(const std::string& input) {
    std::string result;
    
    // Use our optimized isBase64Encoded function to avoid redundant decoding
    if (isBase64Encoded(input, &result)) {
        return result;
    }
    return "";
}

void cleanupTempDir(const std::string& path) {
    // Simple recursive directory removal using system command
    // In production, you'd want a more robust solution
    std::string cmd = "rm -rf " + path;
    system(cmd.c_str());
}

// Controllers
class ApiController : public drogon::HttpController<ApiController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ApiController::processFiles, "/api/process_files", drogon::Post);
    ADD_METHOD_TO(ApiController::processRepo, "/api/process_repo", drogon::Post);
    ADD_METHOD_TO(ApiController::processUploadedDir, "/api/process_uploaded_dir", drogon::Post);
    ADD_METHOD_TO(ApiController::processSharedDir, "/api/process_shared", drogon::Post);
    ADD_METHOD_TO(ApiController::getCapabilities, "/api/capabilities", drogon::Get);
    ADD_METHOD_TO(ApiController::getContentFile, "/api/content/{filename}", drogon::Get);
    METHOD_LIST_END

    void processFiles(const drogon::HttpRequestPtr& req, 
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        // Use Json::Value for Drogon responses
        Json::Value drogonResult;
        json result; // nlohmann::json for internal processing
        
        auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);

        try {
            std::cout << "Processing files request..." << std::endl;
            
            // Check if request is multipart
            drogon::MultiPartParser fileUpload;
            if (!fileUpload.parse(req)) {
                std::cerr << "Error: Invalid request format, not multipart data" << std::endl;
                result["success"] = false;
                result["error"] = "Invalid request format. Expected multipart form data.";
                
                // Convert nlohmann::json to Json::Value for the response
                drogonResult["success"] = false;
                drogonResult["error"] = "Invalid request format. Expected multipart form data.";
                
                resp->setStatusCode(drogon::k400BadRequest);
                callback(resp);
                return;
            }

            // Create temp directory
            std::string tempDir = createTempDir();
            std::cout << "Created temp directory: " << tempDir << std::endl;
            
            // Process uploaded files
            auto& files = fileUpload.getFiles();
            std::cout << "Received " << files.size() << " files" << std::endl;
            for (auto& file : files) {
                // Get file content and save to temp dir
                std::string filePath = tempDir + "/" + file.getFileName();
                std::ofstream fileOut(filePath, std::ios::binary);
                if (fileOut) {
                    // Check if the content appears to be Base64-encoded
                    std::string fileContent(file.fileContent().data(), file.fileContent().size());
                    if (isBase64Encoded(fileContent)) {
                        // Decode the Base64 content
                        std::string decodedContent = decodeBase64(fileContent);
                        fileOut.write(decodedContent.data(), decodedContent.size());
                    } else {
                        // Write as-is
                        fileOut.write(file.fileContent().data(), file.fileContent().size());
                    }
                    fileOut.close();
                } else {
                    std::cerr << "Failed to create file: " << filePath << std::endl;
                }
            }

            // Extract format option
            std::string formatStr = "plain";
            if (req->getJsonObject() && req->getJsonObject()->isMember("format")) {
                formatStr = (*req->getJsonObject())["format"].asString();
            }
            
            // Extract other options
            bool verbose = false;
            if (req->getJsonObject() && req->getJsonObject()->isMember("verbose")) {
                verbose = (*req->getJsonObject())["verbose"].asBool();
            }
            
            bool showTiming = false;
            if (req->getJsonObject() && req->getJsonObject()->isMember("timing")) {
                showTiming = (*req->getJsonObject())["timing"].asBool();
            }
            
            // Parse summarization options
            SummarizationOptions summarizationOptions;
            if (req->getJsonObject() && req->getJsonObject()->isMember("summarization")) {
                auto& summaryJson = (*req->getJsonObject())["summarization"];
                
                if (summaryJson.isMember("enabled")) {
                    summarizationOptions.enabled = summaryJson["enabled"].asBool();
                }
                
                if (summaryJson.isMember("includeFirstNLines")) {
                    summarizationOptions.includeFirstNLines = summaryJson["includeFirstNLines"].asBool();
                }
                
                if (summaryJson.isMember("firstNLinesCount")) {
                    summarizationOptions.firstNLinesCount = summaryJson["firstNLinesCount"].asInt();
                }
                
                if (summaryJson.isMember("includeSignatures")) {
                    summarizationOptions.includeSignatures = summaryJson["includeSignatures"].asBool();
                }
                
                if (summaryJson.isMember("includeDocstrings")) {
                    summarizationOptions.includeDocstrings = summaryJson["includeDocstrings"].asBool();
                }
                
                if (summaryJson.isMember("includeSnippets")) {
                    summarizationOptions.includeSnippets = summaryJson["includeSnippets"].asBool();
                }
                
                if (summaryJson.isMember("snippetsCount")) {
                    summarizationOptions.snippetsCount = summaryJson["snippetsCount"].asInt();
                }
                
                if (summaryJson.isMember("includeReadme")) {
                    summarizationOptions.includeReadme = summaryJson["includeReadme"].asBool();
                }
                
                if (summaryJson.isMember("useTreeSitter")) {
                    summarizationOptions.useTreeSitter = summaryJson["useTreeSitter"].asBool();
                }
                
                if (summaryJson.isMember("fileSizeThreshold")) {
                    summarizationOptions.fileSizeThreshold = summaryJson["fileSizeThreshold"].asUInt();
                }
                
                if (summaryJson.isMember("maxSummaryLines")) {
                    summarizationOptions.maxSummaryLines = summaryJson["maxSummaryLines"].asInt();
                }
                
                // Parse the new NER options
                if (summaryJson.isMember("includeEntityRecognition")) {
                    summarizationOptions.includeEntityRecognition = summaryJson["includeEntityRecognition"].asBool();
                }
                
                if (summaryJson.isMember("includeClassNames")) {
                    summarizationOptions.includeClassNames = summaryJson["includeClassNames"].asBool();
                }
                
                if (summaryJson.isMember("includeFunctionNames")) {
                    summarizationOptions.includeFunctionNames = summaryJson["includeFunctionNames"].asBool();
                }
                
                if (summaryJson.isMember("includeVariableNames")) {
                    summarizationOptions.includeVariableNames = summaryJson["includeVariableNames"].asBool();
                }
                
                if (summaryJson.isMember("includeEnumValues")) {
                    summarizationOptions.includeEnumValues = summaryJson["includeEnumValues"].asBool();
                }
                
                if (summaryJson.isMember("includeImports")) {
                    summarizationOptions.includeImports = summaryJson["includeImports"].asBool();
                }
                
                if (summaryJson.isMember("maxEntities")) {
                    summarizationOptions.maxEntities = summaryJson["maxEntities"].asInt();
                }
                
                if (summaryJson.isMember("groupEntitiesByType")) {
                    summarizationOptions.groupEntitiesByType = summaryJson["groupEntitiesByType"].asBool();
                }

                // Parse NER method
                if (summaryJson.isMember("nerMethod")) {
                    std::string nerMethod = summaryJson["nerMethod"].asString();
                    if (nerMethod == "Regex") {
                        summarizationOptions.nerMethod = SummarizationOptions::NERMethod::Regex;
                    } else if (nerMethod == "TreeSitter") {
                        summarizationOptions.nerMethod = SummarizationOptions::NERMethod::TreeSitter;
                    } else if (nerMethod == "ML") {
                        summarizationOptions.nerMethod = SummarizationOptions::NERMethod::ML;
                    } else if (nerMethod == "Hybrid") {
                        summarizationOptions.nerMethod = SummarizationOptions::NERMethod::Hybrid;
                    }
                }

                // Parse ML-NER specific options
                if (summaryJson.isMember("useMLForLargeFiles")) {
                    summarizationOptions.useMLForLargeFiles = summaryJson["useMLForLargeFiles"].asBool();
                }

                if (summaryJson.isMember("mlNerSizeThreshold")) {
                    summarizationOptions.mlNerSizeThreshold = summaryJson["mlNerSizeThreshold"].asUInt();
                }

                if (summaryJson.isMember("mlModelPath")) {
                    summarizationOptions.mlModelPath = summaryJson["mlModelPath"].asString();
                }

                if (summaryJson.isMember("cacheMLResults")) {
                    summarizationOptions.cacheMLResults = summaryJson["cacheMLResults"].asBool();
                }

                if (summaryJson.isMember("mlConfidenceThreshold")) {
                    summarizationOptions.mlConfidenceThreshold = summaryJson["mlConfidenceThreshold"].asFloat();
                }

                if (summaryJson.isMember("maxMLProcessingTimeMs")) {
                    summarizationOptions.maxMLProcessingTimeMs = summaryJson["maxMLProcessingTimeMs"].asInt();
                }

                // Parse advanced visualization options
                if (summaryJson.isMember("includeEntityRelationships")) {
                    summarizationOptions.includeEntityRelationships = summaryJson["includeEntityRelationships"].asBool();
                }

                if (summaryJson.isMember("generateEntityGraph")) {
                    summarizationOptions.generateEntityGraph = summaryJson["generateEntityGraph"].asBool();
                }
            }
            
            // Set options for repomix
            RepomixOptions options;
            options.inputDir = tempDir;
            options.format = formatStr == "markdown" ? OutputFormat::Markdown : 
                             formatStr == "xml" ? OutputFormat::XML :
                             formatStr == "claude_xml" ? OutputFormat::ClaudeXML :
                             OutputFormat::Plain;
            options.verbose = verbose;
            options.showTiming = showTiming;
            options.summarization = summarizationOptions;  // Set summarization options
            
            // Process the uploaded files
            Repomix repomix(options);
            bool success = repomix.run();
            
            // Format result as JSON
            result["success"] = success;
            result["summary"] = repomix.getSummary();
            
            if (success) {
                result["content"] = repomix.getOutput();
            } else {
                result["error"] = "Failed to process files";
            }
            
            // Clean up temp directory
            cleanupTempDir(tempDir);
            
            // Convert to Json::Value for the response
            drogonResult["success"] = success;
            drogonResult["summary"] = repomix.getSummary();
            
            if (success) {
                // Get the output content as a string
                std::string outputContent = repomix.getOutput();
                drogonResult["content"] = outputContent;
            } else {
                // Get the error as a string
                std::string errorMsg = result["error"].is_string() ? 
                                      result["error"].get<std::string>() : 
                                      "Error occurred";
                drogonResult["error"] = errorMsg;
            }
            
            resp->setStatusCode(drogon::k200OK);
            
        } catch (const std::exception& e) {
            result["success"] = false;
            result["error"] = e.what();
            
            // Convert nlohmann::json to Json::Value for the response
            drogonResult["success"] = false;
            drogonResult["error"] = e.what();
            
            resp->setStatusCode(drogon::k500InternalServerError);
        }
        
        callback(resp);
    }

    void processRepo(const drogon::HttpRequestPtr& req, 
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        // Use Json::Value for Drogon responses
        Json::Value drogonResult;
        json result; // nlohmann::json for internal processing
        json body;
        
        try {
            std::cout << "Processing repository request..." << std::endl;
            
            // Parse request body
            auto bodyStr = req->getBody();
            if (bodyStr.empty()) {
                std::cerr << "Error: Empty request body" << std::endl;
                result["success"] = false;
                result["error"] = "Request body is empty";
                
                // Convert to Json::Value for response
                drogonResult["success"] = false;
                drogonResult["error"] = "Request body is empty";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
                resp->setStatusCode(drogon::k400BadRequest);
                callback(resp);
                return;
            }
            
            try {
                body = json::parse(bodyStr);
            } catch (const json::exception& e) {
                std::cerr << "Error parsing JSON: " << e.what() << std::endl;
                result["success"] = false;
                result["error"] = "Invalid JSON: " + std::string(e.what());
                
                // Convert to Json::Value for response
                drogonResult["success"] = false;
                drogonResult["error"] = "Invalid JSON: " + std::string(e.what());
                auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
                resp->setStatusCode(drogon::k400BadRequest);
                callback(resp);
                return;
            }
            
            // Extract repo URL
            if (!body.contains("repoUrl") || !body["repoUrl"].is_string()) {
                result["success"] = false;
                result["error"] = "Missing or invalid repoUrl in request body";
                
                // Convert nlohmann::json to Json::Value for the response
                drogonResult["success"] = false;
                drogonResult["error"] = "Missing or invalid repoUrl in request body";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
                resp->setStatusCode(drogon::k400BadRequest);
                callback(resp);
                return;
            }
            
            std::string repoUrl = body["repoUrl"];
            
            // Create temp directory
            std::string tempDir = createTempDir();
            
            // Clone the repository
            std::string cloneCmd = "git clone --depth=1 " + repoUrl + " " + tempDir;
            int cloneResult = system(cloneCmd.c_str());
            
            if (cloneResult != 0) {
                result["success"] = false;
                result["error"] = "Failed to clone repository: " + repoUrl;
                cleanupTempDir(tempDir);
                
                // Convert nlohmann::json to Json::Value for the response
                drogonResult["success"] = false;
                drogonResult["error"] = "Failed to clone repository: " + repoUrl;
                auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
                resp->setStatusCode(drogon::k500InternalServerError);
                callback(resp);
                return;
            }
            
            // Create repomix options
            RepomixOptions options;
            options.inputDir = tempDir;
            options.verbose = false;
            options.numThreads = std::thread::hardware_concurrency();
            
            // Set output format
            std::string format = "plain";
            if (body.contains("format") && body["format"].is_string()) {
                format = body["format"];
            }
            
            if (format == "markdown") {
                options.format = OutputFormat::Markdown;
            } else if (format == "xml") {
                options.format = OutputFormat::XML;
            } else if (format == "claude_xml") {
                options.format = OutputFormat::ClaudeXML;
            } else {
                options.format = OutputFormat::Plain;
            }
            
            // Don't write to a file in server mode
            options.outputFile = "";
            
            // Process repository
            Repomix repomix(options);
            bool success = repomix.run();
            
            // Get outputs before preparing response
            std::string summary = repomix.getSummary();
            std::string outputContent = repomix.getOutput();

            std::cout << "Repository processing " << (success ? "successful" : "failed") << std::endl;
            std::cout << "Summary length: " << summary.length() << " bytes" << std::endl;
            std::cout << "Content length: " << outputContent.length() << " bytes" << std::endl;

            // For large content, save to file instead of sending in JSON response
            const size_t LARGE_CONTENT_THRESHOLD = 3 * 1024 * 1024; // 3MB
            bool contentInFile = false;
            std::string contentFilePath;

            // If content is large, write to file
            if (outputContent.length() > LARGE_CONTENT_THRESHOLD) {
                std::cout << "Content is large (" << outputContent.length() << " bytes), saving to file" << std::endl;
                
                // If the content is too long, write it to a file
                auto timestamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
                
                if (options.format == OutputFormat::Markdown) {
                    contentFilePath = SHARED_DIRECTORY + "/repomix_content_" + timestamp + ".md";
                } else {
                    contentFilePath = SHARED_DIRECTORY + "/repomix_content_" + timestamp + ".txt";
                }
                
                std::ofstream outFile(contentFilePath, std::ios::binary);
                if (outFile) {
                    // For newly generated content, we know it's not base64 encoded
                    // so we don't need to check or decode it
                    std::cout << "First 100 chars of content: " << outputContent.substr(0, 100) << std::endl;
                    outFile.write(outputContent.c_str(), outputContent.length());
                    outFile.close();
                    contentInFile = true;
                    
                    // Extract just the filename part for the response
                    size_t lastSlash = contentFilePath.find_last_of('/');
                    if (lastSlash != std::string::npos) {
                        contentFilePath = contentFilePath.substr(lastSlash + 1);
                    }
                    
                    std::cout << "Content saved to: " << contentFilePath << std::endl;
                } else {
                    std::cerr << "Failed to write content to file, will include in response" << std::endl;
                    contentInFile = false;
                }
            }

            // Prepare response
            drogonResult["success"] = success;
            drogonResult["summary"] = summary;
            drogonResult["contentInFile"] = contentInFile;
            if (contentInFile) {
                drogonResult["contentFilePath"] = contentFilePath;
                // Add a brief content snippet
                drogonResult["contentSnippet"] = outputContent.substr(0, 1000) + "...\n[Full content available in file]";
            }

            if (success) {
                if (outputContent.empty()) {
                    std::cout << "WARNING: Output content is empty!" << std::endl;
                    // Provide a fallback message instead of empty content
                    drogonResult["content"] = "Repository processed successfully, but no content was generated.";
                } else if (!contentInFile) {
                    // Check if the content is Base64 encoded before adding it to response
                    std::string decodedContent;
                    bool isBase64 = isBase64Encoded(outputContent, &decodedContent);
                    
                    if (isBase64) {
                        std::cout << "Response content is Base64 encoded, decoding before sending..." << std::endl;
                        // Use the already decoded content from isBase64Encoded
                        drogonResult["content"] = decodedContent;
                    } else {
                        // Content is not Base64 encoded, use as is
                        drogonResult["content"] = outputContent;
                    }
                }
            } else {
                drogonResult["error"] = "Failed to process repository";
            }

            // Log response info
            std::cout << "Response status: " << (success ? "success" : "failure") << std::endl;
            std::cout << "Response has content: " << (drogonResult.isMember("content") ? "yes" : "no") << std::endl;
            std::cout << "Response has error: " << (drogonResult.isMember("error") ? "yes" : "no") << std::endl;
            auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
            resp->setStatusCode(drogon::k200OK);
            
            // Clean up temp directory
            cleanupTempDir(tempDir);
            
        } catch (const json::exception& e) {
            result["success"] = false;
            result["error"] = "Invalid JSON: " + std::string(e.what());
            
            // Convert nlohmann::json to Json::Value for the response
            drogonResult["success"] = false;
            drogonResult["error"] = "Invalid JSON: " + std::string(e.what());
            auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
            resp->setStatusCode(drogon::k400BadRequest);
        } catch (const std::exception& e) {
            result["success"] = false;
            result["error"] = e.what();
            
            // Convert nlohmann::json to Json::Value for the response
            drogonResult["success"] = false;
            drogonResult["error"] = e.what();
            auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
            resp->setStatusCode(drogon::k500InternalServerError);
        }
        
        auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
        callback(resp);
    }

    void getCapabilities(const drogon::HttpRequestPtr& req, 
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        json capabilities;
        capabilities["version"] = "1.0.0";
        capabilities["formats"] = json::array({"plain", "markdown", "xml", "claude_xml"});
        capabilities["features"] = json::array({"file_upload", "github_repo", "timing_info"});
        
        // Add summarization features
        json summarizationFeatures;
        summarizationFeatures["enabled"] = true;
        summarizationFeatures["options"] = {
            {"includeFirstNLines", true},
            {"firstNLinesCount", 50},
            {"includeSignatures", true},
            {"includeDocstrings", true},
            {"includeSnippets", true},
            {"snippetsCount", 3},
            {"includeReadme", true},
            {"useTreeSitter", true},
            {"fileSizeThreshold", 10240},
            {"maxSummaryLines", 200}
        };
        
        capabilities["summarization"] = summarizationFeatures;
        
        // Convert nlohmann::json to Json::Value for Drogon
        Json::Value drogonJson;
        drogonJson["version"] = capabilities["version"].get<std::string>();
        
        // Add formats array
        Json::Value formatsArray;
        for (const auto& format : capabilities["formats"]) {
            formatsArray.append(format.get<std::string>());
        }
        drogonJson["formats"] = formatsArray;
        
        // Add features array
        Json::Value featuresArray;
        for (const auto& feature : capabilities["features"]) {
            featuresArray.append(feature.get<std::string>());
        }
        drogonJson["features"] = featuresArray;
        
        // Add summarization features
        Json::Value summaryJson;
        summaryJson["enabled"] = summarizationFeatures["enabled"].get<bool>();
        
        Json::Value optionsJson;
        for (const auto& [key, value] : summarizationFeatures["options"].items()) {
            if (value.is_boolean()) {
                optionsJson[key] = value.get<bool>();
            } else if (value.is_number_integer()) {
                optionsJson[key] = value.get<int>();
            }
        }
        summaryJson["options"] = optionsJson;
        drogonJson["summarization"] = summaryJson;
        
        auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonJson);
        callback(resp);
    }

    // New method to process directories that are uploaded from the frontend
    void processUploadedDir(const drogon::HttpRequestPtr& req, 
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        // Use Json::Value for Drogon responses
        Json::Value drogonResult;
        json result; // nlohmann::json for internal processing

        try {
            std::cout << "Processing directory from frontend upload..." << std::endl;
            
            // Check if request is multipart
            drogon::MultiPartParser fileUpload;
            if (!fileUpload.parse(req)) {
                std::cerr << "Error: Invalid request format, not multipart data" << std::endl;
                drogonResult["success"] = false;
                drogonResult["error"] = "Invalid request format. Expected multipart form data.";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
                resp->setStatusCode(drogon::k400BadRequest);
                callback(resp);
                return;
            }

            // Create temp directory
            std::string tempDir = createTempDir();
            std::cout << "Created temp directory: " << tempDir << std::endl;
            
            // Process uploaded files - maintain directory structure if possible
            auto& files = fileUpload.getFiles();
            std::cout << "Received " << files.size() << " files" << std::endl;
            
            for (auto& file : files) {
                // Get file path from form field name or content-disposition
                std::string relPath = file.getFileName();
                
                // Create the directory structure if needed
                fs::path filePath = fs::path(tempDir) / relPath;
                fs::create_directories(filePath.parent_path());
                
                // Save file content
                std::ofstream fileOut(filePath, std::ios::binary);
                if (fileOut) {
                    // Check if the content appears to be Base64-encoded
                    std::string fileContent(file.fileContent().data(), file.fileContent().size());
                    if (isBase64Encoded(fileContent)) {
                        // Decode the Base64 content
                        std::string decodedContent = decodeBase64(fileContent);
                        fileOut.write(decodedContent.data(), decodedContent.size());
                    } else {
                        // Write as-is
                        fileOut.write(file.fileContent().data(), file.fileContent().size());
                    }
                    fileOut.close();
                } else {
                    std::cerr << "Failed to create file: " << filePath << std::endl;
                }
                
                std::cout << "Saved file: " << relPath << " (" << file.fileContent().size() << " bytes)" << std::endl;
            }
            
            // Process the directory with Repomix
            RepomixOptions options;
            options.inputDir = tempDir;
            options.format = OutputFormat::Plain; // Default - can be overridden by form params
            
            // Check for format parameter
            auto params = req->getParameters();
            if (params.find("format") != params.end()) {
                std::string format = params["format"];
                if (format == "markdown") {
                    options.format = OutputFormat::Markdown;
                } else if (format == "xml") {
                    options.format = OutputFormat::XML;
                } else if (format == "claude_xml") {
                    options.format = OutputFormat::ClaudeXML;
                }
            }
            
            // Process the directory
            Repomix repomix(options);
            bool success = repomix.run();
            
            // Clean up temp directory
            cleanupTempDir(tempDir);
            
            // Prepare response
            drogonResult["success"] = success;
            drogonResult["summary"] = repomix.getSummary();
            
            if (success) {
                std::string outputContent = repomix.getOutput();
                drogonResult["content"] = outputContent;
            } else {
                drogonResult["error"] = "Failed to process directory";
            }
            auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
            resp->setStatusCode(drogon::k200OK);
            
        } catch (const std::exception& e) {
            std::cerr << "Error processing directory: " << e.what() << std::endl;
            drogonResult["success"] = false;
            drogonResult["error"] = e.what();
            auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
            resp->setStatusCode(drogon::k500InternalServerError);
        }
        auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
        callback(resp);
    }

    // Process files from a shared directory
    void processSharedDir(const drogon::HttpRequestPtr& req, 
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        // Use Json::Value for Drogon responses
        Json::Value drogonResult;
        json result; // nlohmann::json for internal processing

        try {
            std::cout << "Processing GitHub repository request..." << std::endl;
            
            // Parse request body
            auto bodyStr = req->getBody();
            if (bodyStr.empty()) {
                std::cerr << "Error: Empty request body" << std::endl;
                drogonResult["success"] = false;
                drogonResult["error"] = "Request body is empty";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
                resp->setStatusCode(drogon::k400BadRequest);
                callback(resp);
                return;
            }
            
            // Parse the JSON body
            json body;
            try {
                body = json::parse(bodyStr);
            } catch (const json::exception& e) {
                std::cerr << "Error parsing JSON: " << e.what() << std::endl;
                drogonResult["success"] = false;
                drogonResult["error"] = "Invalid JSON: " + std::string(e.what());
                auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
                resp->setStatusCode(drogon::k400BadRequest);
                callback(resp);
                return;
            }
            
            // Extract the repository URL
            if (!body.contains("repoUrl") || !body["repoUrl"].is_string()) {
                std::cerr << "Error: Missing or invalid repoUrl in request body" << std::endl;
                drogonResult["success"] = false;
                drogonResult["error"] = "Missing or invalid repoUrl in request body";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
                resp->setStatusCode(drogon::k400BadRequest);
                callback(resp);
                return;
            }
            
            std::string repoUrl = body["repoUrl"].get<std::string>();
            
            // Extract format if provided
            std::string formatStr = "plain";
            OutputFormat format = OutputFormat::Plain;
            
            if (body.contains("format") && body["format"].is_string()) {
                formatStr = body["format"].get<std::string>();
                if (formatStr == "markdown") {
                    format = OutputFormat::Markdown;
                } else if (formatStr == "xml") {
                    format = OutputFormat::XML;
                } else if (formatStr == "claude_xml") {
                    format = OutputFormat::ClaudeXML;
                } else {
                    format = OutputFormat::Plain;
                }
            }
            
            // Extract GitHub token if provided
            std::string token;
            if (body.contains("token") && body["token"].is_string()) {
                token = body["token"].get<std::string>();
            }
            
            // Parse the GitHub URL to extract owner and repo
            std::regex githubUrlPattern("github\\.com\\/([^\\/]+)\\/([^\\/]+)");
            std::smatch matches;
            if (!std::regex_search(repoUrl, matches, githubUrlPattern) || matches.size() < 3) {
                std::cerr << "Error: Invalid GitHub repository URL format" << std::endl;
                drogonResult["success"] = false;
                drogonResult["error"] = "Invalid GitHub repository URL format";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
                resp->setStatusCode(drogon::k400BadRequest);
                callback(resp);
                return;
            }
            
            std::string owner = matches[1];
            std::string repo = matches[2];
            
            // Create a temporary directory to store the repository
            std::string tempDir = createTempDir();
            std::cout << "Created temp directory: " << tempDir << std::endl;
            
            // Initialize CURL
            CURL* curl = curl_easy_init();
            if (!curl) {
                std::cerr << "Error: Failed to initialize CURL" << std::endl;
                drogonResult["success"] = false;
                drogonResult["error"] = "Failed to initialize CURL";
                cleanupTempDir(tempDir);
                auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
                resp->setStatusCode(drogon::k500InternalServerError);
                callback(resp);
                return;
            }
            
            // Set common CURL options
            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, "Accept: application/vnd.github.v3+json");
            headers = curl_slist_append(headers, "User-Agent: Repomix-Server");
            
            if (!token.empty()) {
                std::string authHeader = "Authorization: token " + token;
                headers = curl_slist_append(headers, authHeader.c_str());
            }
            
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            
            // First, fetch the repository file tree
            std::string treeUrl = "https://api.github.com/repos/" + owner + "/" + repo + "/git/trees/HEAD?recursive=1";
            std::string treeResponse;
            
            curl_easy_setopt(curl, CURLOPT_URL, treeUrl.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &treeResponse);
            
            CURLcode treeRes = curl_easy_perform(curl);
            if (treeRes != CURLE_OK) {
                std::cerr << "Error: Failed to fetch repository tree: " << curl_easy_strerror(treeRes) << std::endl;
                drogonResult["success"] = false;
                drogonResult["error"] = "Failed to fetch repository tree: " + std::string(curl_easy_strerror(treeRes));
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
                cleanupTempDir(tempDir);
                auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
                resp->setStatusCode(drogon::k500InternalServerError);
                callback(resp);
                return;
            }
            
            // Parse the tree JSON
            json treeData;
            try {
                treeData = json::parse(treeResponse);
            } catch (const json::exception& e) {
                std::cerr << "Error parsing tree JSON: " << e.what() << std::endl;
                drogonResult["success"] = false;
                drogonResult["error"] = "Failed to parse repository tree: " + std::string(e.what());
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
                cleanupTempDir(tempDir);
                auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
                resp->setStatusCode(drogon::k500InternalServerError);
                callback(resp);
                return;
            }
            
            // Check if the tree exists and contains files
            if (!treeData.contains("tree") || !treeData["tree"].is_array()) {
                std::cerr << "Error: Invalid tree data format" << std::endl;
                drogonResult["success"] = false;
                drogonResult["error"] = "Invalid tree data format";
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
                cleanupTempDir(tempDir);
                auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
                resp->setStatusCode(drogon::k500InternalServerError);
                callback(resp);
                return;
            }
            
            // Filter for blob (file) entries only
            std::vector<json> files;
            for (const auto& item : treeData["tree"]) {
                if (item.contains("type") && item["type"] == "blob" && 
                    item.contains("path") && item.contains("url")) {
                    files.push_back(item);
                }
            }
            
            std::cout << "Found " << files.size() << " files in repository" << std::endl;
            
            // Limit to a reasonable number of files to avoid overloading
            const size_t MAX_FILES = 100;
            if (files.size() > MAX_FILES) {
                std::cout << "Limiting to " << MAX_FILES << " files" << std::endl;
                files.resize(MAX_FILES);
            }
            
            // Fetch each file
            int fileCount = 0;
            for (const auto& file : files) {
                std::string filePath = file["path"].get<std::string>();
                std::string blobUrl = "https://api.github.com/repos/" + owner + "/" + repo + "/contents/" + filePath;
                
                std::string fileResponse;
                curl_easy_setopt(curl, CURLOPT_URL, blobUrl.c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fileResponse);
                
                CURLcode fileRes = curl_easy_perform(curl);
                if (fileRes != CURLE_OK) {
                    std::cerr << "Warning: Failed to fetch file " << filePath << ": " << curl_easy_strerror(fileRes) << std::endl;
                    continue; // Skip this file but continue with others
                }
                
                // Parse the file JSON
                json fileData;
                try {
                    fileData = json::parse(fileResponse);
                } catch (const json::exception& e) {
                    std::cerr << "Warning: Failed to parse file data for " << filePath << ": " << e.what() << std::endl;
                    continue; // Skip this file but continue with others
                }
                
                // Check if file content is available
                if (!fileData.contains("content") || !fileData.contains("encoding") || 
                    fileData["encoding"] != "base64") {
                    std::cerr << "Warning: Invalid content format for " << filePath << std::endl;
                    continue; // Skip this file but continue with others
                }
                
                // Decode base64 content
                std::string base64Content = fileData["content"].get<std::string>();
                
                // Remove newlines from base64 string since GitHub API includes them
                std::string cleanBase64 = base64Content;
                cleanBase64.erase(std::remove(cleanBase64.begin(), cleanBase64.end(), '\n'), cleanBase64.end());
                
                std::cout << "Decoding base64 file content for: " << filePath << std::endl;
                std::cout << "Base64 content length (after cleaning): " << cleanBase64.length() << " bytes" << std::endl;
                
                // Use Drogon's base64 decoder
                try {
                    std::string decoded = drogon::utils::base64Decode(cleanBase64);
                    std::cout << "Decoded content length: " << decoded.length() << " bytes" << std::endl;
                    
                    // Display sample of decoded content for debugging
                    if (decoded.length() > 0) {
                        std::string sampleText;
                        int textChars = 0;
                        for (size_t i = 0; i < std::min(decoded.length(), size_t(100)); i++) {
                            char c = decoded[i];
                            if ((c >= 32 && c <= 126) || c == '\n' || c == '\r' || c == '\t') {
                                sampleText += c;
                                textChars++;
                            } else {
                                sampleText += '.'; // Replace non-printable chars
                            }
                        }
                        
                        double textRatio = static_cast<double>(textChars) / std::min(decoded.length(), size_t(100));
                        std::cout << "Sample of decoded content: " << sampleText << std::endl;
                        std::cout << "Text ratio of sample: " << textRatio << std::endl;
                    }
                    
                    // Create the target file path
                    fs::path targetPath = fs::path(tempDir) / filePath;
                    
                    // Create parent directories if they don't exist
                    fs::create_directories(targetPath.parent_path());
                    
                    // Write the decoded content to the file
                    FILE* file = fopen(targetPath.string().c_str(), "wb");
                    if (file) {
                        fwrite(decoded.c_str(), 1, decoded.length(), file);
                        fileCount++;
                        fclose(file);
                    } else {
                        std::cerr << "Warning: Failed to create file " << targetPath.string() << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error decoding base64 content: " << e.what() << std::endl;
                }
            }
            
            // Clean up CURL resources
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            
            std::cout << "Successfully saved " << fileCount << " files to " << tempDir << std::endl;
            
            // Process the directory with Repomix
            RepomixOptions options;
            options.inputDir = tempDir;
            options.format = format;
            
            // Process the directory
            Repomix repomix(options);
            bool success = repomix.run();
            
            // Get outputs before preparing response
            std::string summary = repomix.getSummary();
            std::string outputContent = repomix.getOutput();

            std::cout << "Repository processing " << (success ? "successful" : "failed") << std::endl;
            std::cout << "Summary length: " << summary.length() << " bytes" << std::endl;
            std::cout << "Content length: " << outputContent.length() << " bytes" << std::endl;

            // For large content, save to file instead of sending in JSON response
            const size_t LARGE_CONTENT_THRESHOLD = 3 * 1024 * 1024; // 3MB
            bool contentInFile = false;
            std::string contentFilePath;

            // If content is large, write to file
            if (outputContent.length() > LARGE_CONTENT_THRESHOLD) {
                std::cout << "Content is large (" << outputContent.length() << " bytes), saving to file" << std::endl;
                
                // If the content is too long, write it to a file
                auto timestamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
                
                if (options.format == OutputFormat::Markdown) {
                    contentFilePath = SHARED_DIRECTORY + "/repomix_content_" + timestamp + ".md";
                } else {
                    contentFilePath = SHARED_DIRECTORY + "/repomix_content_" + timestamp + ".txt";
                }
                
                std::ofstream outFile(contentFilePath, std::ios::binary);
                if (outFile) {
                    // For newly generated content, we know it's not base64 encoded
                    // so we don't need to check or decode it
                    std::cout << "First 100 chars of content: " << outputContent.substr(0, 100) << std::endl;
                    outFile.write(outputContent.c_str(), outputContent.length());
                    outFile.close();
                    contentInFile = true;
                    
                    // Extract just the filename part for the response
                    size_t lastSlash = contentFilePath.find_last_of('/');
                    if (lastSlash != std::string::npos) {
                        contentFilePath = contentFilePath.substr(lastSlash + 1);
                    }
                    
                    std::cout << "Content saved to: " << contentFilePath << std::endl;
                } else {
                    std::cerr << "Failed to write content to file, will include in response" << std::endl;
                    contentInFile = false;
                }
            }

            // Prepare response
            drogonResult["success"] = success;
            drogonResult["summary"] = summary;
            drogonResult["contentInFile"] = contentInFile;
            if (contentInFile) {
                drogonResult["contentFilePath"] = contentFilePath;
                // Add a brief content snippet
                drogonResult["contentSnippet"] = outputContent.substr(0, 1000) + "...\n[Full content available in file]";
            }

            if (success) {
                if (outputContent.empty()) {
                    std::cout << "WARNING: Output content is empty!" << std::endl;
                    // Provide a fallback message instead of empty content
                    drogonResult["content"] = "Repository processed successfully, but no content was generated.";
                } else if (!contentInFile) {
                    // Check if the content is Base64 encoded before adding it to response
                    std::string decodedContent;
                    bool isBase64 = isBase64Encoded(outputContent, &decodedContent);
                    
                    if (isBase64) {
                        std::cout << "Response content is Base64 encoded, decoding before sending..." << std::endl;
                        // Use the already decoded content from isBase64Encoded
                        drogonResult["content"] = decodedContent;
                    } else {
                        // Content is not Base64 encoded, use as is
                        drogonResult["content"] = outputContent;
                    }
                }
            } else {
                drogonResult["error"] = "Failed to process repository";
            }

            // Log response info
            std::cout << "Response status: " << (success ? "success" : "failure") << std::endl;
            std::cout << "Response has content: " << (drogonResult.isMember("content") ? "yes" : "no") << std::endl;
            std::cout << "Response has error: " << (drogonResult.isMember("error") ? "yes" : "no") << std::endl;
            auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
            resp->setStatusCode(drogon::k200OK);
            
            // Clean up the temporary directory
            cleanupTempDir(tempDir);
            
        } catch (const std::exception& e) {
            std::cerr << "Error processing GitHub repository: " << e.what() << std::endl;
            drogonResult["success"] = false;
            drogonResult["error"] = e.what();
            auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
            resp->setStatusCode(drogon::k500InternalServerError);
        }
        auto resp = drogon::HttpResponse::newHttpJsonResponse(drogonResult);
        callback(resp);
    }

    // Serve content files
    void getContentFile(const drogon::HttpRequestPtr& req, 
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                        const std::string& filename) {
        std::cout << "Request for content file: " << filename << std::endl;
        
        // Sanitize filename to prevent directory traversal attacks
        // Allow dots for file extensions, but filter out other potentially dangerous characters
        std::string safeFilename = filename;
        safeFilename.erase(std::remove_if(safeFilename.begin(), safeFilename.end(), 
                         [](char c) { return c == '/' || c == '\\' || c == ' '; }), 
                         safeFilename.end());
        
        // Additional check for ".." which could be used for directory traversal
        if (safeFilename.find("..") != std::string::npos) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeString("text/plain");
            resp->setBody("Invalid filename");
            callback(resp);
            return;
        }
        
        if (safeFilename.empty() || safeFilename != filename) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeString("text/plain");
            resp->setBody("Invalid filename");
            callback(resp);
            return;
        }
        
        // Construct full file path
        std::string filePath = SHARED_DIRECTORY + "/" + safeFilename;
        
        // Check if file exists
        if (!fs::exists(filePath) || !fs::is_regular_file(filePath)) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeString("text/plain");
            resp->setBody("File not found");
            callback(resp);
            return;
        }
        
        // Check if the file might be Base64 encoded - but skip for .txt and .md files
        // as we're now properly saving them without encoding
        std::ifstream fileCheck(filePath, std::ios::binary);
        if (fileCheck) {
            // Get the file extension
            std::string extension;
            size_t dotPos = safeFilename.find_last_of(".");
            if (dotPos != std::string::npos) {
                extension = safeFilename.substr(dotPos + 1);
            }
            
            // For repomix content files, always serve them directly
            if (safeFilename.find("repomix_content_") == 0 && (extension == "txt" || extension == "md")) {
                std::cout << "Serving repomix content file directly: " << safeFilename << std::endl;
                auto resp = drogon::HttpResponse::newFileResponse(filePath);
                resp->addHeader("Content-Disposition", "attachment; filename=\"" + safeFilename + "\"");
                resp->addHeader("Access-Control-Allow-Origin", "*");
                resp->addHeader("Access-Control-Allow-Methods", "GET");
                callback(resp);
                return;
            }
            
            // Read the file content for other types
            std::string fileContent;
            fileCheck.seekg(0, std::ios::end);
            fileContent.resize(fileCheck.tellg());
            fileCheck.seekg(0, std::ios::beg);
            fileCheck.read(&fileContent[0], fileContent.size());
            fileCheck.close();
            
            // For file types that might be base64 encoded
            bool isTextFile = (extension == "txt" || extension == "md" || extension == "json" || 
                              extension == "html" || extension == "css" || extension == "js");
            
            // Only attempt base64 decoding for files we know might be encoded
            // Most .txt files produced by our system are not base64 encoded
            bool attemptDecode = !isTextFile && fileContent.size() > 0;
            
            if (attemptDecode && isBase64Encoded(fileContent)) {
                // Try to decode
                std::string decodedContent;
                try {
                    decodedContent = decodeBase64(fileContent);
                    
                    if (decodedContent.empty()) {
                        // If decoding fails, fall back to serving the original file
                        auto resp = drogon::HttpResponse::newFileResponse(filePath);
                        resp->addHeader("Content-Disposition", "attachment; filename=\"" + safeFilename + "\"");
                        resp->addHeader("Access-Control-Allow-Origin", "*");
                        resp->addHeader("Access-Control-Allow-Methods", "GET");
                        callback(resp);
                        return;
                    }
                } catch (...) {
                    // If decoding throws an exception, fall back to serving the original file
                    auto resp = drogon::HttpResponse::newFileResponse(filePath);
                    resp->addHeader("Content-Disposition", "attachment; filename=\"" + safeFilename + "\"");
                    resp->addHeader("Access-Control-Allow-Origin", "*");
                    resp->addHeader("Access-Control-Allow-Methods", "GET");
                    callback(resp);
                    return;
                }
                
                // Attempt to detect the content type based on the decoded content
                std::string contentType = "text/plain";
                
                // Simple content type detection based on first few bytes or common patterns
                if (!decodedContent.empty()) {
                    if (decodedContent.size() >= 4 && 
                        decodedContent[0] == 'P' && decodedContent[1] == 'K' && 
                        decodedContent[2] == 3 && decodedContent[3] == 4) {
                        contentType = "application/zip";
                    } else if (decodedContent.size() >= 5 && 
                               decodedContent.substr(0, 5) == "%PDF-") {
                        contentType = "application/pdf";
                    } else if (decodedContent.size() >= 4 && 
                               (decodedContent.substr(0, 4) == "\x89PNG" || 
                                decodedContent.substr(0, 2) == "BM" || 
                                decodedContent.substr(0, 3) == "GIF")) {
                        contentType = "image/octet-stream";
                    }
                    // Add more content type detection as needed
                }
                
                // Create a response with the decoded content
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setBody(decodedContent);
                resp->setContentTypeString(contentType);
                resp->addHeader("Content-Disposition", "attachment; filename=\"" + safeFilename + "\"");
                resp->addHeader("Access-Control-Allow-Origin", "*");
                resp->addHeader("Access-Control-Allow-Methods", "GET");
                callback(resp);
                return;
            }
        }
        
        // Create a file response for non-Base64 content
        auto resp = drogon::HttpResponse::newFileResponse(filePath);
        
        // Set content disposition to force download
        resp->addHeader("Content-Disposition", "attachment; filename=\"" + safeFilename + "\"");
        
        // Set CORS headers
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "GET");
        callback(resp);
    }
};

int main(int argc, char* argv[]) {
    try {
        std::cout << "=== Repomix API Server Starting ===\n";
        
        // Create logs directory if it doesn't exist
        try {
            if (!fs::exists("./logs")) {
                std::cout << "Creating logs directory...\n";
                fs::create_directory("./logs");
            }
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to create logs directory: " << e.what() << std::endl;
        }
        
        // Check for SHARED_DIR environment variable
        const char* sharedDirEnv = std::getenv("SHARED_DIR");
        if (sharedDirEnv) {
            SHARED_DIRECTORY = sharedDirEnv;
            std::cout << "Using shared directory from environment: " << SHARED_DIRECTORY << std::endl;
        } else {
            std::cout << "Using default shared directory: " << SHARED_DIRECTORY << std::endl;
        }
        
        // Create shared directory if it doesn't exist
        try {
            if (!fs::exists(SHARED_DIRECTORY)) {
                std::cout << "Creating shared directory: " << SHARED_DIRECTORY << "...\n";
                fs::create_directories(SHARED_DIRECTORY);
                // Set permissions (chmod 777 equivalent)
                fs::permissions(SHARED_DIRECTORY, 
                    fs::perms::owner_all | fs::perms::group_all | fs::perms::others_all);
            }
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to create or set permissions on shared directory: " 
                     << e.what() << std::endl;
        }
        
        // Parse command line arguments for port
        int port = 9000;  // Default port
        std::string host = "0.0.0.0";  // Default host
        
        std::cout << "Parsing command line arguments...\n";
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--port" && i + 1 < argc) {
                port = std::stoi(argv[++i]);
                std::cout << "Custom port specified: " << port << std::endl;
            } else if (arg == "--host" && i + 1 < argc) {
                host = argv[++i];
                std::cout << "Custom host specified: " << host << std::endl;
            }
        }
        
        std::cout << "Server will listen on " << host << ":" << port << std::endl;
        std::cout << "Using " << std::thread::hardware_concurrency() << " threads\n";
        
        // Configure Drogon app
        std::cout << "Configuring Drogon app...\n";
        auto& app = drogon::app()
            .setLogLevel(trantor::Logger::kDebug)
            .setLogPath("./logs")
            .addListener(host, port)
            .setThreadNum(std::thread::hardware_concurrency())
            .setDocumentRoot("./frontend")
            .setMaxConnectionNum(100000)
            .setIdleConnectionTimeout(60);
        
        // Add CORS handlers for both normal and OPTIONS requests
        std::cout << "Setting up CORS support...\n";
        
        // Handle OPTIONS requests for CORS preflight
        app.registerHandler(
            "/api/{1}",
            [](const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback,
               const std::string&) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k204NoContent);
                resp->addHeader("Access-Control-Allow-Origin", "*");
                resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
                resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
                resp->addHeader("Access-Control-Max-Age", "86400");
                callback(resp);
            },
            {drogon::Options}
        );
        
        // Also register the specific paths to be extra safe
        for (const auto& path : {"/api/process_files", "/api/process_repo", 
                                  "/api/process_uploaded_dir", "/api/process_shared", 
                                  "/api/capabilities"}) {
            app.registerHandler(
                path,
                [](const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k204NoContent);
                    resp->addHeader("Access-Control-Allow-Origin", "*");
                    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
                    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
                    resp->addHeader("Access-Control-Max-Age", "86400");
                    callback(resp);
                },
                {drogon::Options}
            );
        }

        // Add CORS headers to all responses
        app.registerPostHandlingAdvice(
            [](const drogon::HttpRequestPtr&, const drogon::HttpResponsePtr& resp) {
                resp->addHeader("Access-Control-Allow-Origin", "*");
                resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
                resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
                resp->addHeader("Access-Control-Max-Age", "86400");
            });

        // Set up application logging
        app.setLogPath("./logs");

        // Skip the custom request logging and rely on our logs in the handler methods
        std::cout << "Starting server...\n";
        app.registerHandler(
            "/api/health",
            [](const drogon::HttpRequestPtr&, 
               std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                Json::Value result;
                result["status"] = "ok";
                result["timestamp"] = static_cast<int>(time(nullptr));
                auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
                callback(resp);
            },
            {drogon::Get}
        );
        
        std::cout << "=== Ready to run server ===\n";
        app.run();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "*** FATAL ERROR: Unhandled exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "*** FATAL ERROR: Unknown exception occurred" << std::endl;
        return 2;
    }
} 