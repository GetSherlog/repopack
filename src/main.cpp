#include <iostream>
#include <CLI/CLI.hpp>
#include "repomix.hpp"

int main(int argc, char** argv) {
    try {
        CLI::App app{"Repomix++ - Package repository contents into a single text file"};
        
        RepomixOptions options;
        std::string formatStr = "plain";
        
        // Required input directory
        app.add_option("-i,--input", options.inputDir, "Input directory (required)")
            ->required()
            ->check(CLI::ExistingDirectory);
        
        // Optional output file
        app.add_option("-o,--output", options.outputFile, "Output file (default: repomix-output.txt)");
        
        // Optional output format
        app.add_option("-f,--format", formatStr, "Output format: plain, markdown, xml (default: plain)")
            ->check(CLI::IsMember({"plain", "markdown", "xml"}));
        
        // Optional verbose flag
        app.add_flag("-v,--verbose", options.verbose, "Enable verbose output");
        
        // Optional timing flag
        app.add_flag("-t,--timing", options.showTiming, "Show detailed timing information");
        
        // Optional thread count
        app.add_option("--threads", options.numThreads, "Number of threads to use for processing (default: number of CPU cores)")
            ->check(CLI::Range(1u, 32u));
        
        // Parse command line arguments
        CLI11_PARSE(app, argc, argv);
        
        // Convert format string to enum
        if (formatStr == "markdown") {
            options.format = OutputFormat::Markdown;
        } else if (formatStr == "xml") {
            options.format = OutputFormat::XML;
        } else {
            options.format = OutputFormat::Plain;
        }
        
        // Run repomix
        Repomix repomix(options);
        if (!repomix.run()) {
            return 1;
        }
        
        // Print summary
        if (options.verbose) {
            std::cout << repomix.getSummary() << std::endl;
        }
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}