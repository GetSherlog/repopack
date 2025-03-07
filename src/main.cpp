#include <iostream>
#include <CLI/CLI.hpp>
#include "repomix.hpp"
#include "tokenizer.hpp"

int main(int argc, char** argv) {
    try {
        CLI::App app{"Repomix++ - Package repository contents into a single text file"};
        
        RepomixOptions options;
        std::string formatStr = "plain";
        std::string includePatterns;
        std::string excludePatterns;
        std::string tokenEncodingStr = "cl100k_base";
        
        // Required input directory
        app.add_option("-i,--input", options.inputDir, "Input directory (required)")
            ->required()
            ->check(CLI::ExistingDirectory);
        
        // Optional output file
        app.add_option("-o,--output", options.outputFile, "Output file (default: repomix-output.txt)");
        
        // Optional output format
        app.add_option("-f,--format", formatStr, "Output format: plain, markdown, xml, claude_xml (default: plain)")
            ->check(CLI::IsMember({"plain", "markdown", "xml", "claude_xml"}));
        
        // Optional include patterns
        app.add_option("--include", includePatterns, 
                     "Comma-separated list of glob patterns for files to include (e.g. *.rs,*.toml)");
        
        // Optional exclude patterns
        app.add_option("--exclude", excludePatterns, 
                     "Comma-separated list of glob patterns for files to exclude (e.g. *.txt,*.md)");
        
        // Optional verbose flag
        app.add_flag("-v,--verbose", options.verbose, "Enable verbose output");
        
        // Optional timing flag
        app.add_flag("-t,--timing", options.showTiming, "Show detailed timing information");
        
        // Optional thread count
        app.add_option("--threads", options.numThreads, "Number of threads to use for processing (default: number of CPU cores)")
            ->check(CLI::Range(1u, 32u));
        
        // Token counting options
        app.add_flag("--tokens", options.countTokens, "Display token count of the generated prompt");
        
        // Token count only (no output generation)
        app.add_flag("--tokens-only", options.onlyShowTokenCount, "Only display token count without generating the full output")
            ->needs("--tokens");
        
        // Tokenizer encoding option
        app.add_option("--encoding", tokenEncodingStr, 
                      "Specify a tokenizer for token count (default: cl100k_base)")
            ->check(CLI::IsMember({"cl100k_base", "cl100k", "p50k_base", "p50k", "p50k_edit", "r50k_base", "r50k", "gpt2", "o200k_base", "o200k"}))
            ->needs("--tokens");
            
        // Add a help section for tokenizers
        app.footer(
            "Supported tokenizers:\n"
            "  cl100k_base  - ChatGPT models, text-embedding-ada-002\n"
            "  p50k_base    - Code models, text-davinci-002, text-davinci-003\n"
            "  p50k_edit    - Edit models like text-davinci-edit-001, code-davinci-edit-001\n"
            "  r50k_base    - GPT-3 models like davinci (alias: gpt2)\n"
            "  o200k_base   - GPT-4o models\n"
        );
        
        // Parse command line arguments
        CLI11_PARSE(app, argc, argv);
        
        // Convert format string to enum
        if (formatStr == "markdown") {
            options.format = OutputFormat::Markdown;
        } else if (formatStr == "xml") {
            options.format = OutputFormat::XML;
        } else if (formatStr == "claude_xml") {
            options.format = OutputFormat::ClaudeXML;
        } else {
            options.format = OutputFormat::Plain;
        }
        
        // Store include/exclude patterns
        options.includePatterns = includePatterns;
        options.excludePatterns = excludePatterns;
        
        // Store tokenizer encoding
        if (options.countTokens) {
            options.tokenEncoding = Tokenizer::encodingFromString(tokenEncodingStr);
        }
        
        // Run repomix
        Repomix repomix(options);
        if (!repomix.run()) {
            return 1;
        }
        
        // Print summary or token count
        if (options.verbose || options.countTokens) {
            std::cout << repomix.getSummary() << std::endl;
        }
        
        // If only showing token count, print that specifically
        if (options.onlyShowTokenCount && options.countTokens) {
            std::cout << repomix.getTokenCount() << std::endl;
        }
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}