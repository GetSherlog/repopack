# Repomix Frontend

This is the frontend web application for Repomix, a high-performance C++ tool for packaging repository contents into a single text file for AI consumption.

## Technology Stack

- **Framework**: Next.js 14 with TypeScript
- **Styling**: Tailwind CSS
- **UI Components**: Custom components with Material UI icons
- **File Handling**: react-dropzone for file uploads, isomorphic-git for GitHub integration
- **Performance**: C++ processing core compiled to WebAssembly

## Features

- Upload local files and directories
- Process GitHub repositories by URL
- High-performance file processing
- File filtering based on .gitignore rules
- Download or copy the processed output

## Directory Structure

```
frontend/
├── app/                  # Next.js app router
│   ├── page.tsx          # Home page
│   ├── upload/           # File upload page
│   └── github/           # GitHub URL page
├── components/           # React components
│   ├── FileUploader.tsx  # File upload component
│   ├── GitHubForm.tsx    # GitHub URL form
│   ├── ProcessingStatus.tsx # Processing status display
│   └── ResultDisplay.tsx # Result display component
├── lib/                  # Utility functions
│   └── repomix-wasm.ts   # WASM integration for Repomix C++
├── public/               # Static assets
│   └── repomix.wasm      # WebAssembly compiled from C++
└── package.json          # Project dependencies
```

## Getting Started

1. Install dependencies:

```bash
npm install
```

2. Run the development server:

```bash
npm run dev
```

3. Open [http://localhost:3000](http://localhost:3000) in your browser.

## WASM Integration

The frontend interfaces with the Repomix C++ core through WebAssembly. The C++ code is compiled to WASM using Emscripten and loaded in the browser. The integration works as follows:

1. Files are uploaded through the UI or cloned from GitHub
2. Files are processed by the WASM module
3. The resulting text output is displayed to the user

## Building for Production

```bash
npm run build
```

Then to start the production server:

```bash
npm start
```

## License

This project is licensed under the MIT License.