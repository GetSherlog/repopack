# Fetch Catch2 library
include(FetchContent)
FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(catch2)

# Add test executable
add_executable(repomix_tests
    main_test.cpp
    file_processor_test.cpp
    pattern_matcher_test.cpp
    ${CMAKE_SOURCE_DIR}/src/file_processor.cpp
    ${CMAKE_SOURCE_DIR}/src/pattern_matcher.cpp
)


# Link against Catch2 and source files
target_link_libraries(repomix_tests PRIVATE 
    Catch2::Catch2WithMain
    CLI11::CLI11
    nlohmann_json::nlohmann_json
)

# Include test sources and register tests
include(${catch2_SOURCE_DIR}/extras/Catch.cmake)
catch_discover_tests(repomix_tests)