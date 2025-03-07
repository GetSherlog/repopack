cmake_minimum_required(VERSION 3.14)
project(repomix VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add compiler warnings
if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
  # Ignore certain warnings in dependencies
  add_compile_options(-Wno-deprecated-declarations -Wno-unused-parameter)
endif()

# Add include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Dependencies
include(FetchContent)

# CLI11 for command-line argument parsing
FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.3.2
)
FetchContent_MakeAvailable(cli11)

# nlohmann/json for configuration file parsing
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# Find system libraries
find_package(Drogon REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

# Add subdirectories for source code
add_subdirectory(src)

# Add test subdirectory
add_subdirectory(test)

# Find dependencies
find_package(Threads REQUIRED)
find_package(nlohmann_json QUIET)
find_package(CLI11 QUIET)

# ONNX Runtime for ML-based NER
option(USE_ONNX_RUNTIME "Enable ONNX Runtime for ML-based NER" ON)
if(USE_ONNX_RUNTIME)
    # Try to find ONNX Runtime
    find_package(ONNXRuntime QUIET)
    
    if(NOT ONNXRuntime_FOUND)
        message(STATUS "ONNX Runtime not found, downloading...")
        set(ONNXRUNTIME_VERSION "1.15.1")
        
        if(WIN32)
            set(ONNX_RUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-win-x64-${ONNXRUNTIME_VERSION}.zip")
            set(ONNX_RUNTIME_HASH "SHA256=1e17c1c8e54b19c583edcc3ffad7e9d581f3b4bcd886904940a1b0e3b58ab10e")
        elseif(APPLE)
            set(ONNX_RUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-osx-x86_64-${ONNXRUNTIME_VERSION}.tgz")
            set(ONNX_RUNTIME_HASH "SHA256=efa4db0ef7c4e52d8e7a2bd91374f0039c84a48a9bbc8dec25ca72d6420c5bf7")
        else()
            set(ONNX_RUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-linux-x64-${ONNXRUNTIME_VERSION}.tgz")
            set(ONNX_RUNTIME_HASH "SHA256=5fc6942e4e4b898c7fb6e4a4f8ebf2cf770310a9fa6de3b1e2b57fdf12862240")
        endif()
        
        FetchContent_Declare(
            onnxruntime
            URL ${ONNX_RUNTIME_URL}
            URL_HASH ${ONNX_RUNTIME_HASH}
        )
        FetchContent_MakeAvailable(onnxruntime)
        
        # Set up include directories and libraries
        set(ONNX_RUNTIME_INCLUDE_DIRS "${onnxruntime_SOURCE_DIR}/include")
        if(WIN32)
            set(ONNX_RUNTIME_LIBRARIES "${onnxruntime_SOURCE_DIR}/lib/onnxruntime.lib")
        else()
            set(ONNX_RUNTIME_LIBRARIES "${onnxruntime_SOURCE_DIR}/lib/libonnxruntime.so")
            if(APPLE)
                set(ONNX_RUNTIME_LIBRARIES "${onnxruntime_SOURCE_DIR}/lib/libonnxruntime.dylib")
            endif()
        endif()
        
        add_compile_definitions(USE_ONNX_RUNTIME)
    endif()
endif()

# Also download the CodeBERT model
add_custom_target(download_models ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/models
    COMMAND curl -o ${CMAKE_BINARY_DIR}/models/codebert-ner.onnx -L https://github.com/microsoft/CodeBERT/raw/master/models/codebert-ner.onnx
    COMMAND curl -o ${CMAKE_BINARY_DIR}/models/vocab.txt -L https://github.com/microsoft/CodeBERT/raw/master/roberta-base/vocab.txt
    COMMENT "Downloading CodeBERT NER model and vocabulary"
)