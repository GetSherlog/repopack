# Create a library for the core functionality
add_library(repomix_lib STATIC
    repomix.cpp
    file_processor.cpp
    pattern_matcher.cpp
    code_ner.cpp
    file_scorer.cpp
)

# Link required libraries to the core library
target_link_libraries(repomix_lib PUBLIC 
    ${CMAKE_THREAD_LIBS_INIT}
    nlohmann_json::nlohmann_json
)

# Add ONNX Runtime if enabled
if(USE_ONNX_RUNTIME)
    target_include_directories(repomix_lib PRIVATE ${ONNX_RUNTIME_INCLUDE_DIRS})
    target_link_libraries(repomix_lib PUBLIC ${ONNX_RUNTIME_LIBRARIES})
    add_dependencies(repomix_lib download_models)
endif()

# Main CLI executable
add_executable(repomix
    main.cpp
    repomix.cpp
    file_processor.cpp
    pattern_matcher.cpp
    code_ner.cpp
    server.cpp
    tokenizer.cpp
    file_scorer.cpp
)

target_include_directories(repomix PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${CLI11_SOURCE_DIR}/include
    ${nlohmann_json_SOURCE_DIR}/include
)

target_link_libraries(repomix PRIVATE
    Threads::Threads
    Drogon::Drogon
    jsoncpp
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
)

# Link tiktoken if enabled
if(USE_TIKTOKEN)
    add_dependencies(repomix tiktoken_build)
    target_link_libraries(repomix PRIVATE ${TIKTOKEN_LIB})
endif()

# Link ONNX Runtime if found
if(ONNXRuntime_FOUND OR USE_ONNX_RUNTIME)
    if(ONNXRuntime_FOUND)
        target_include_directories(repomix PRIVATE ${ONNXRuntime_INCLUDE_DIRS})
        target_link_libraries(repomix PRIVATE ${ONNXRuntime_LIBRARIES})
    else()
        target_include_directories(repomix PRIVATE ${ONNX_RUNTIME_INCLUDE_DIRS})
        target_link_libraries(repomix PRIVATE ${ONNX_RUNTIME_LIBRARIES})
    endif()
    add_compile_definitions(USE_ONNX_RUNTIME)
    add_dependencies(repomix download_models)
endif()

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
)

# Installation
install(TARGETS repomix
    RUNTIME DESTINATION bin
)

install(TARGETS repomix_server DESTINATION bin)