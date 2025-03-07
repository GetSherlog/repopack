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
        std::string fileSelectionStr = "all";
        
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
        app.add_flag("--count-tokens", options.countTokens, "Count tokens in the output");
        app.add_option("--token-encoding", tokenEncodingStr, "Token encoding to use: cl100k_base, r50k_base, p50k_base (default: cl100k_base)")
            ->check(CLI::IsMember({"cl100k_base", "r50k_base", "p50k_base"}));
        app.add_flag("--only-show-token-count", options.onlyShowTokenCount, "Only show token count without generating full output");
        
        // File selection strategy options
        auto fileSelectionOpt = app.add_option("--file-selection", fileSelectionStr, 
                                        "File selection strategy: all, scoring (default: all)")
            ->check(CLI::IsMember({"all", "scoring"}));
            
        // File scoring options (only used when selection is 'scoring')
        auto scoringGroup = app.add_option_group("Scoring Options");
        
        // Project structure weights
        scoringGroup->add_option("--root-files-weight", options.scoringConfig.rootFilesWeight, 
                               "Weight for root-level files (default: 0.9)");
        scoringGroup->add_option("--top-level-dirs-weight", options.scoringConfig.topLevelDirsWeight, 
                               "Weight for top-level directories (default: 0.8)");
        scoringGroup->add_option("--entry-points-weight", options.scoringConfig.entryPointsWeight, 
                               "Weight for entry point files (default: 0.8)");
        scoringGroup->add_option("--dependency-graph-weight", options.scoringConfig.dependencyGraphWeight, 
                               "Weight for dependency graph connectivity (default: 0.7)");
        
        // File type weights
        scoringGroup->add_option("--source-code-weight", options.scoringConfig.sourceCodeWeight, 
                               "Weight for source code files (default: 0.8)");
        scoringGroup->add_option("--config-files-weight", options.scoringConfig.configFilesWeight, 
                               "Weight for configuration files (default: 0.7)");
        scoringGroup->add_option("--documentation-weight", options.scoringConfig.documentationWeight, 
                               "Weight for documentation files (default: 0.6)");
        scoringGroup->add_option("--test-files-weight", options.scoringConfig.testFilesWeight, 
                               "Weight for test files (default: 0.5)");
        
        // Recency weights
        scoringGroup->add_option("--recent-files-weight", options.scoringConfig.recentlyModifiedWeight, 
                               "Weight for recently modified files (default: 0.7)");
        scoringGroup->add_option("--recent-time-window", options.scoringConfig.recentTimeWindowDays, 
                               "Window in days for recent files (default: 7)");
        
        // File size weights
        scoringGroup->add_option("--file-size-weight", options.scoringConfig.fileSizeWeight, 
                               "Weight for file size (inverse, smaller files score higher) (default: 0.4)");
        scoringGroup->add_option("--large-file-threshold", options.scoringConfig.largeFileThreshold, 
                               "Threshold in bytes for large files (default: 1000000)");
        
        // Code density
        scoringGroup->add_option("--code-density-weight", options.scoringConfig.codeDensityWeight, 
                               "Weight for code density (default: 0.5)");
        
        // Inclusion threshold
        scoringGroup->add_option("--inclusion-threshold", options.scoringConfig.inclusionThreshold, 
                               "Minimum score for file inclusion (default: 0.3)");
        
        // TreeSitter usage
        scoringGroup->add_flag("--use-tree-sitter", options.scoringConfig.useTreeSitter, 
                               "Use TreeSitter for improved parsing (default: true)");
        
        // Generate file scoring report
        bool generateScoringReport = false;
        std::string scoringReportPath;
        scoringGroup->add_flag("--scoring-report", generateScoringReport, 
                               "Generate file scoring report");
        scoringGroup->add_option("--scoring-report-path", scoringReportPath, 
                               "Path for scoring report file (default: scoring-report.json)");
                               
        // Only enable scoring options when scoring is selected
        fileSelectionOpt->needs(scoringGroup);
        
        // Parse command line arguments
        CLI11_PARSE(app, argc, argv);
        
        // Set output format
        if (formatStr == "plain") {
            options.format = OutputFormat::Plain;
        } else if (formatStr == "markdown") {
            options.format = OutputFormat::Markdown;
        } else if (formatStr == "xml") {
            options.format = OutputFormat::XML;
        } else if (formatStr == "claude_xml") {
            options.format = OutputFormat::ClaudeXML;
        }
        
        // Set include and exclude patterns
        options.includePatterns = includePatterns;
        options.excludePatterns = excludePatterns;
        
        // Set token encoding
        if (options.countTokens) {
            options.tokenEncoding = Tokenizer::encodingFromString(tokenEncodingStr);
        }
        
        // Set file selection strategy
        if (fileSelectionStr == "scoring") {
            options.selectionStrategy = RepomixOptions::FileSelectionStrategy::Scoring;
        } else {
            options.selectionStrategy = RepomixOptions::FileSelectionStrategy::All;
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
        
        // Generate scoring report if requested
        if (generateScoringReport && options.selectionStrategy == RepomixOptions::FileSelectionStrategy::Scoring) {
            std::string reportPath = scoringReportPath.empty() ? "scoring-report.json" : scoringReportPath;
            std::string report = repomix.getFileScoringReport();
            
            std::ofstream reportFile(reportPath);
            if (reportFile) {
                reportFile << report;
                if (options.verbose) {
                    std::cout << "Scoring report written to " << reportPath << std::endl;
                }
            } else {
                std::cerr << "Error: Failed to write scoring report to " << reportPath << std::endl;
            }
        }
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}