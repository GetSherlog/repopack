# Create a library for the core functionality
add_library(repomix_lib STATIC
    repomix.cpp
    file_processor.cpp
    pattern_matcher.cpp
    code_ner.cpp
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
)

# Link the core library to the CLI executable
target_link_libraries(repomix PRIVATE 
    repomix_lib
    CLI11::CLI11 
    nlohmann_json::nlohmann_json
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
)

# Installation
install(TARGETS repomix DESTINATION bin)
install(TARGETS repomix_server DESTINATION bin)