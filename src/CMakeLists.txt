# Create a library for the core functionality
add_library(repomix_lib STATIC
    repomix.cpp
    file_processor.cpp
    pattern_matcher.cpp
    code_ner.cpp
    file_scorer.cpp
    tokenizer.cpp
)

# Link required libraries to the core library
target_link_libraries(repomix_lib PUBLIC 
    ${CMAKE_THREAD_LIBS_INIT}
    nlohmann_json::nlohmann_json
    tree-sitter
)

# Link tiktoken if enabled
if(USE_TIKTOKEN)
    if(TARGET tiktoken::tiktoken)
        target_link_libraries(repomix_lib PUBLIC tiktoken::tiktoken)
    else()
        message(WARNING "tiktoken target not found, linking directly with ${CPP_TIKTOKEN_LIBRARY}")
        target_link_libraries(repomix_lib PUBLIC ${CPP_TIKTOKEN_LIBRARY})
        # Link against dependencies
        if(TARGET fmt::fmt)
            target_link_libraries(repomix_lib PUBLIC fmt::fmt)
        elseif(FMT_LIBRARY)
            target_link_libraries(repomix_lib PUBLIC ${FMT_LIBRARY})
        endif()
        
        if(PCRE2_LIBRARY)
            target_link_libraries(repomix_lib PUBLIC ${PCRE2_LIBRARY})
        endif()
    endif()
endif()

# Add language parser libraries
target_link_libraries(repomix_lib PUBLIC
    tree-sitter-cpp
    tree-sitter-c
    tree-sitter-python
    tree-sitter-javascript
)

# For tree-sitter-cpp (assumes scanner is in scanner.c)
target_sources(tree-sitter-cpp PRIVATE
    ${tree-sitter-cpp_SOURCE_DIR}/src/scanner.c
)

# For tree-sitter-python (assumes scanner is in scanner.c)
target_sources(tree-sitter-python PRIVATE
    ${tree-sitter-python_SOURCE_DIR}/src/scanner.c
)

# For tree-sitter-javascript (assumes scanner is in scanner.c)
target_sources(tree-sitter-javascript PRIVATE
    ${tree-sitter-javascript_SOURCE_DIR}/src/scanner.c
)

# Add include directories for TreeSitter
target_include_directories(repomix_lib PRIVATE
    ${tree-sitter_SOURCE_DIR}/lib/include
    ${tree-sitter-cpp_SOURCE_DIR}/src
    ${tree-sitter-c_SOURCE_DIR}/src
    ${tree-sitter-python_SOURCE_DIR}/src
    ${tree-sitter-javascript_SOURCE_DIR}/src
)

# Add ONNX Runtime if enabled
if(USE_ONNX_RUNTIME)
    target_include_directories(repomix_lib PRIVATE ${ONNX_RUNTIME_INCLUDE_DIRS})
    target_link_libraries(repomix_lib PUBLIC ${ONNX_RUNTIME_LIBRARIES})
    add_dependencies(repomix_lib download_models)
endif()

# Link ONNX Runtime if found
if(ONNXRuntime_FOUND OR USE_ONNX_RUNTIME)
    if(ONNXRuntime_FOUND)
        target_include_directories(repomix_lib PRIVATE ${ONNXRuntime_INCLUDE_DIRS})
        target_link_libraries(repomix_lib PUBLIC ${ONNXRuntime_LIBRARIES})
    else()
        target_include_directories(repomix_lib PRIVATE ${ONNX_RUNTIME_INCLUDE_DIRS})
        target_link_libraries(repomix_lib PUBLIC ${ONNX_RUNTIME_LIBRARIES})
    endif()
    add_compile_definitions(USE_ONNX_RUNTIME)
    add_dependencies(repomix_lib download_models)
endif()

# Main CLI executable
add_executable(repomix
    main.cpp
)

target_include_directories(repomix PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${CLI11_SOURCE_DIR}/include
    ${nlohmann_json_SOURCE_DIR}/include
)

# Add Tree-sitter include directories
target_include_directories(repomix PRIVATE
    ${tree-sitter_SOURCE_DIR}/lib/include
    ${tree-sitter-cpp_SOURCE_DIR}/src
    ${tree-sitter-c_SOURCE_DIR}/src
    ${tree-sitter-python_SOURCE_DIR}/src
    ${tree-sitter-javascript_SOURCE_DIR}/src
)

target_link_libraries(repomix PRIVATE
    repomix_lib
    Threads::Threads
    Drogon::Drogon
    jsoncpp
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
    tree-sitter
)

# Add language parser libraries 
target_link_libraries(repomix PRIVATE
    tree-sitter-cpp
    tree-sitter-c
    tree-sitter-python
    tree-sitter-javascript
)

# Server executable with Drogon
add_executable(repomix_server
    server.cpp
)

# Link Drogon and the core library to the server executable
target_link_libraries(repomix_server PRIVATE
    repomix_lib
    Drogon::Drogon
    nlohmann_json::nlohmann_json
    CURL::libcurl
    tree-sitter
)

# Add language parser libraries
target_link_libraries(repomix_server PRIVATE
    tree-sitter-cpp
    tree-sitter-c
    tree-sitter-python
    tree-sitter-javascript
)

# Target include directories for server
target_include_directories(repomix_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
)

# Add Tree-sitter include directories
target_include_directories(repomix_server PRIVATE
    ${tree-sitter_SOURCE_DIR}/lib/include
    ${tree-sitter-cpp_SOURCE_DIR}/src
    ${tree-sitter-c_SOURCE_DIR}/src
    ${tree-sitter-python_SOURCE_DIR}/src
    ${tree-sitter-javascript_SOURCE_DIR}/src
)

# Installation
install(TARGETS repomix
    RUNTIME DESTINATION bin
)

install(TARGETS repomix_server DESTINATION bin)