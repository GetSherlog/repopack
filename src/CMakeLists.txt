# Create a library for the core functionality
add_library(repomix_lib STATIC
    repomix.cpp
    file_processor.cpp
    pattern_matcher.cpp
)

# Link required libraries to the core library
target_link_libraries(repomix_lib PRIVATE 
    nlohmann_json::nlohmann_json
)

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