# repopack-cpp

repopack-cpp is a high-performance, C++-based tool designed to package the contents of a software repository (local or remote) into a single, AI-friendly text file. This file is intended for consumption by large language models (LLMs) for tasks such as code review, documentation generation, architecture analysis, and other AI-assisted development workflows.

## Features

- Package repository content into a single text file for LLM consumption
- Extremely fast processing of large codebases with:
  - Multithreaded file processing
  - Memory-mapped I/O for large files
  - Optimized buffer management and string operations
- Support for .gitignore patterns to exclude files
- Intelligent default ignore patterns for build artifacts, dependencies, and non-essential files
- Multiple output formats (plain text, Markdown, XML)
- Directory structure visualization
- File summary statistics
- Performance metrics and timing information
- Token counting for LLM inputs with multiple tokenizer support
- Web frontend with file upload and GitHub repository support
- C++ backend server with RESTful API

## Repository Structure

```
repopack-cpp/
├── include/               # Header files
│   ├── file_processor.hpp # File processing with multithreading
│   ├── pattern_matcher.hpp # Pattern matching
│   └── repopack.hpp        # Main orchestration class
├── src/                   # Implementation files
│   ├── file_processor.cpp
│   ├── main.cpp           # CLI entry point
│   ├── pattern_matcher.cpp
│   ├── repopack.cpp
│   └── server.cpp         # API server implementation
├── test/                  # Test files
│   ├── file_processor_test.cpp
│   ├── main_test.cpp
│   └── pattern_matcher_test.cpp
├── frontend/              # Web frontend (Next.js)
│   ├── app/               # Next.js app router
│   ├── components/        # React components
│   ├── lib/               # Utility functions
│   └── public/            # Static assets
└── build/                 # Build output
```

## Server Architecture

repopack-cpp uses a C++ backend server with a Next.js frontend:

1. **Backend**: C++ server built with the Drogon framework that processes repositories and files
2. **Frontend**: Next.js application that provides a user interface for interacting with the backend

The backend exposes the following API endpoints:
- `/api/process_files` - Process uploaded files
- `/api/process_repo` - Process a git repository by URL
- `/api/capabilities` - Get server capabilities information

## Getting Started

### Using Docker

repopack-cpp is designed to run with Docker, which handles all dependencies automatically:

```bash
# Make the script executable (if needed)
chmod +x docker-start.sh

# Start the application with Docker
./docker-start.sh
```

This will:
1. Build Docker images for both backend and frontend
2. Start containers for both services
3. Make the application available at http://localhost:3000

### Docker Commands

Some useful Docker commands for managing repopack-cpp:

```bash
# View logs from both services
docker-compose logs -f

# Stop the application
docker-compose down

# Restart services
docker-compose restart

# Rebuild and restart (after code changes)
docker-compose up --build -d
```

## Usage

### Web Interface

Once the application is running, you can access the web interface at http://localhost:3000 to:

- Upload local files and directories
- Process GitHub repositories by URL
- Download or copy the processed output

### Command-line Interface

The Docker container also includes a command-line interface for processing repositories:

```bash
# Enter the backend container
docker-compose exec backend bash

# Process a local directory
./build/bin/repopack --input /path/to/repository --output output.txt

# Use markdown format with timing information
./build/bin/repopack --input /path/to/repository --format markdown --timing

# Enable verbose output
./build/bin/repopack --input /path/to/repository --verbose
```

### Command-line Options

#### Input Source
- `-i, --input`: Local directory path to process (required)

#### Output Options
- `-o, --output`: Output file (default: repopack-output.txt)
- `-f, --format`: Output format: plain, markdown, xml (default: plain)

#### Token Counting Options
- `--tokens`: Display token count of the generated prompt
- `--tokens-only`: Only display token count without generating the full output
- `--encoding`: Specify a tokenizer for token count (default: cl100k_base)
  - Supported tokenizers:
    - `cl100k_base` - ChatGPT models, text-embedding-ada-002
    - `p50k_base` - Code models, text-davinci-002, text-davinci-003
    - `p50k_edit` - Edit models like text-davinci-edit-001, code-davinci-edit-001
    - `r50k_base` - GPT-3 models like davinci (alias: gpt2)
    - `o200k_base` - GPT-4o models

#### Other Options
- `-v, --verbose`: Enable verbose output
- `-t, --timing`: Show detailed timing information
- `--threads`: Number of threads to use for processing (default: number of CPU cores)

### Token Counting Examples

```bash
# Display token count with default cl100k tokenizer
./build/bin/repopack --input /path/to/repository --tokens

# Specify a different tokenizer
./build/bin/repopack --input /path/to/repository --tokens --encoding=p50k_base

# Only show token count without generating output
./build/bin/repopack --input /path/to/repository --tokens-only
```

## Performance

Current benchmarks:
- ~660 files per second
- ~158,000 lines per second
- ~9.5 MB per second

## License

MIT

## Acknowledgements

This project is a reimplementation and enhancement of the original [repopack](https://github.com/yamadashy/repopack) tool written in TypeScript.