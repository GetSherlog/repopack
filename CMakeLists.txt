cmake_minimum_required(VERSION 3.14)
project(repomix VERSION 0.1.0 LANGUAGES C CXX)

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

# Find system libraries
find_package(Threads REQUIRED)
find_package(Drogon REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

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

# TreeSitter for code parsing
# First check if ICU is available
find_package(ICU COMPONENTS uc data QUIET)

# Set options for tree-sitter build
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build tree-sitter as a static library")

# Set compile definition globally for tree-sitter
add_compile_definitions(TREE_SITTER_UTF8=0)

# Use locally copied tree-sitter repositories
message(STATUS "Using locally copied tree-sitter repositories")

# Set tree-sitter source directory
set(tree-sitter_SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/tree-sitter-repos/tree-sitter)
set(tree-sitter-cpp_SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/tree-sitter-repos/tree-sitter-cpp)
set(tree-sitter-c_SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/tree-sitter-repos/tree-sitter-c)
set(tree-sitter-python_SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/tree-sitter-repos/tree-sitter-python)
set(tree-sitter-javascript_SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/tree-sitter-repos/tree-sitter-javascript)

# Build TreeSitter library
add_library(tree-sitter STATIC
    ${tree-sitter_SOURCE_DIR}/lib/src/lib.c
)
set_target_properties(tree-sitter PROPERTIES LINKER_LANGUAGE C)
target_include_directories(tree-sitter PUBLIC
    ${tree-sitter_SOURCE_DIR}/lib/include
)
target_compile_definitions(tree-sitter PRIVATE TREE_SITTER_UTF8=0)

# Link with ICU if it's available
if(ICU_FOUND)
    target_link_libraries(tree-sitter PRIVATE ICU::uc ICU::data)
    target_include_directories(tree-sitter PRIVATE ${ICU_INCLUDE_DIRS})
else()
    message(STATUS "ICU library not found, using Unicode compatibility mode")
    # Ensure we're not trying to use ICU
    target_compile_definitions(tree-sitter PRIVATE TREE_SITTER_UTF8=0)
endif()

# Build TreeSitter language parsers
add_library(tree-sitter-cpp STATIC
    ${tree-sitter-cpp_SOURCE_DIR}/src/parser.c
)
set_target_properties(tree-sitter-cpp PROPERTIES LINKER_LANGUAGE C)
target_include_directories(tree-sitter-cpp PUBLIC
    ${tree-sitter-cpp_SOURCE_DIR}/src
    ${tree-sitter_SOURCE_DIR}/lib/include
)

add_library(tree-sitter-c STATIC
    ${tree-sitter-c_SOURCE_DIR}/src/parser.c
)
set_target_properties(tree-sitter-c PROPERTIES LINKER_LANGUAGE C)
target_include_directories(tree-sitter-c PUBLIC
    ${tree-sitter-c_SOURCE_DIR}/src
    ${tree-sitter_SOURCE_DIR}/lib/include
)

add_library(tree-sitter-python STATIC
    ${tree-sitter-python_SOURCE_DIR}/src/parser.c
)
set_target_properties(tree-sitter-python PROPERTIES LINKER_LANGUAGE C)
target_include_directories(tree-sitter-python PUBLIC
    ${tree-sitter-python_SOURCE_DIR}/src
    ${tree-sitter_SOURCE_DIR}/lib/include
)

add_library(tree-sitter-javascript STATIC
    ${tree-sitter-javascript_SOURCE_DIR}/src/parser.c
)
set_target_properties(tree-sitter-javascript PROPERTIES LINKER_LANGUAGE C)
target_include_directories(tree-sitter-javascript PUBLIC
    ${tree-sitter-javascript_SOURCE_DIR}/src
    ${tree-sitter_SOURCE_DIR}/lib/include
)

# tiktoken for tokenization
option(USE_TIKTOKEN "Enable tiktoken for token counting" ON)

if(USE_TIKTOKEN)
    # Find fmt library (dependency of cpp-tiktoken)
    find_package(fmt QUIET)
    if(NOT fmt_FOUND)
        message(STATUS "Looking for fmt library in system paths...")
        find_library(FMT_LIBRARY NAMES fmt)
        find_path(FMT_INCLUDE_DIR NAMES fmt/core.h)
        
        if(FMT_LIBRARY AND FMT_INCLUDE_DIR)
            message(STATUS "Found fmt: ${FMT_LIBRARY}")
            include_directories(${FMT_INCLUDE_DIR})
        else()
            message(WARNING "fmt library not found, tiktoken may not build correctly")
        endif()
    endif()
    
    # Find PCRE2 library (dependency of cpp-tiktoken)
    find_path(PCRE2_INCLUDE_DIR pcre2.h
        PATHS /usr/include /usr/local/include
        DOC "The directory where pcre2.h resides")
    
    find_library(PCRE2_LIBRARY
        NAMES pcre2-8
        PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu
        DOC "The PCRE2 library")
    
    if(PCRE2_INCLUDE_DIR AND PCRE2_LIBRARY)
        message(STATUS "Found PCRE2: ${PCRE2_LIBRARY}")
        include_directories(${PCRE2_INCLUDE_DIR})
    else()
        message(WARNING "PCRE2 library not found, tiktoken may not build correctly")
    endif()

    # Look for cpp-tiktoken library
    find_path(CPP_TIKTOKEN_INCLUDE_DIR tiktoken/encoding.h
        PATHS /usr/local/include
        DOC "The directory where tiktoken/encoding.h resides")
    
    find_library(CPP_TIKTOKEN_LIBRARY
        NAMES tiktoken cpp-tiktoken
        PATHS /usr/local/lib
        DOC "The cpp-tiktoken library")
    
    if(CPP_TIKTOKEN_INCLUDE_DIR AND CPP_TIKTOKEN_LIBRARY)
        message(STATUS "Found cpp-tiktoken: ${CPP_TIKTOKEN_LIBRARY}")
        include_directories(${CPP_TIKTOKEN_INCLUDE_DIR})
        # Set the library as a CMake target so it can be linked against
        if(NOT TARGET tiktoken::tiktoken)
            add_library(tiktoken::tiktoken UNKNOWN IMPORTED)
            set_target_properties(tiktoken::tiktoken PROPERTIES
                IMPORTED_LOCATION "${CPP_TIKTOKEN_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${CPP_TIKTOKEN_INCLUDE_DIR}")
            
            # Link dependencies to the tiktoken target
            if(TARGET fmt::fmt)
                set_property(TARGET tiktoken::tiktoken PROPERTY
                    INTERFACE_LINK_LIBRARIES fmt::fmt)
            endif()
            
            if(PCRE2_LIBRARY)
                set_property(TARGET tiktoken::tiktoken APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES "${PCRE2_LIBRARY}")
            endif()
        endif()
        add_compile_definitions(USE_TIKTOKEN)
    else()
        message(WARNING "cpp-tiktoken not found. Token counting may not work correctly.")
        set(USE_TIKTOKEN OFF)
    endif()
endif()

# Add subdirectories for source code
add_subdirectory(src)

# Add test subdirectory
add_subdirectory(test)

# Find dependencies
find_package(nlohmann_json QUIET)
find_package(CLI11 QUIET)

# ONNX Runtime for ML-based NER
option(USE_ONNX_RUNTIME "Enable ONNX Runtime for ML-based NER" ON)
if(USE_ONNX_RUNTIME)
    # Try to find ONNX Runtime with hints from the environment
    find_library(ONNX_RUNTIME_LIB
        NAMES onnxruntime
        PATHS ${ONNX_RUNTIME_LIB} /usr/local/lib /usr/lib
        DOC "Path to the ONNX Runtime library"
    )
    
    find_path(ONNX_RUNTIME_INCLUDE_DIR
        NAMES onnxruntime_cxx_api.h
        PATHS ${ONNX_RUNTIME_INCLUDE_DIR} /usr/local/include /usr/include
        DOC "Path to the ONNX Runtime include directory"
    )
    
    if(ONNX_RUNTIME_LIB AND ONNX_RUNTIME_INCLUDE_DIR)
        message(STATUS "Found ONNX Runtime: ${ONNX_RUNTIME_LIB}")
        include_directories(${ONNX_RUNTIME_INCLUDE_DIR})
        # Create an imported target for ONNX Runtime
        if(NOT TARGET ONNXRuntime::ONNXRuntime)
            add_library(ONNXRuntime::ONNXRuntime UNKNOWN IMPORTED)
            set_target_properties(ONNXRuntime::ONNXRuntime PROPERTIES
                IMPORTED_LOCATION "${ONNX_RUNTIME_LIB}"
                INTERFACE_INCLUDE_DIRECTORIES "${ONNX_RUNTIME_INCLUDE_DIR}"
            )
        endif()
        add_compile_definitions(USE_ONNX_RUNTIME)
    else()
        message(WARNING "ONNX Runtime not found. ML-based NER will be disabled.")
        set(USE_ONNX_RUNTIME OFF)
    endif()
endif()

# Models are pre-downloaded in the Dockerfile to /app/models
if(USE_ONNX_RUNTIME)
    # Add compile definition to provide path to models
    add_compile_definitions(ONNX_MODEL_PATH="${CMAKE_SOURCE_DIR}/models")
    
    # Check if models directory exists and create it if it doesn't
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/models")
        file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/models")
    endif()
    
    # Check if model files exist and create placeholders if they don't
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/models/codebert-ner.onnx")
        message(STATUS "Model file not found, creating placeholder")
        file(TOUCH "${CMAKE_SOURCE_DIR}/models/codebert-ner.onnx")
    endif()
    
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/models/vocab.txt")
        message(STATUS "Vocab file not found, creating placeholder")
        file(WRITE "${CMAKE_SOURCE_DIR}/models/vocab.txt" "{\"placeholder\": 0}")
    endif()
    
    # Create a symbolic link to the models directory if it doesn't exist
    if(NOT EXISTS "${CMAKE_BINARY_DIR}/models")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E create_symlink "${CMAKE_SOURCE_DIR}/models" "${CMAKE_BINARY_DIR}/models"
            RESULT_VARIABLE LINK_RESULT
        )
        
        if(NOT LINK_RESULT EQUAL 0)
            # If symlink fails, just copy the directory
            message(STATUS "Creating symlink failed, copying model files instead")
            file(COPY "${CMAKE_SOURCE_DIR}/models" DESTINATION "${CMAKE_BINARY_DIR}")
        endif()
    endif()
else()
    message(STATUS "ONNX Runtime is disabled, skipping model setup")
endif()