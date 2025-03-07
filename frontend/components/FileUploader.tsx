'use client'

import { useCallback, useState, useEffect } from 'react'
import { useDropzone } from 'react-dropzone'
import SummarizationOptions, { SummarizationOptions as SummarizationOptionsType, defaultSummarizationOptions } from './SummarizationOptions'
import { TOKENIZER_ENCODINGS } from '../lib/repomix-api'

// Add type declaration for HTML input attributes
declare module 'react' {
  interface InputHTMLAttributes<T> extends HTMLAttributes<T> {
    directory?: string;
    webkitdirectory?: string;
  }
}

interface FileUploaderProps {
  onFilesUploaded: (
    files: File[], 
    includePatterns: string, 
    excludePatterns: string,
    format: string,
    countTokens: boolean,
    tokenEncoding: string,
    tokensOnly: boolean
  ) => void;
  isProcessing: boolean;
}

export default function FileUploader({ onFilesUploaded, isProcessing }: FileUploaderProps) {
  const [files, setFiles] = useState<File[]>([])
  const [includePatterns, setIncludePatterns] = useState<string>('')
  const [excludePatterns, setExcludePatterns] = useState<string>('')
  const [showAdvanced, setShowAdvanced] = useState<boolean>(false)
  const [format, setFormat] = useState<string>('plain')
  const [summarizationOptions, setSummarizationOptions] = useState<SummarizationOptionsType>(defaultSummarizationOptions)
  const [countTokens, setCountTokens] = useState<boolean>(false)
  const [tokenEncoding, setTokenEncoding] = useState<string>('cl100k_base')
  const [tokensOnly, setTokensOnly] = useState<boolean>(false)

  const onDrop = useCallback((acceptedFiles: File[]) => {
    // Handle folder uploads by recursively traversing the FileSystemEntry structure
    // Note: In a real implementation, we'd use the FileSystemAPI, but for simplicity
    // we're just collecting the flat list of files here
    setFiles(prev => [...prev, ...acceptedFiles])
  }, [])

  const { getRootProps, getInputProps, isDragActive } = useDropzone({
    onDrop,
    disabled: isProcessing,
    multiple: true,
    noClick: files.length > 0
  })

  const removeFile = (index: number) => {
    setFiles(prev => prev.filter((_, i) => i !== index))
  }

  // Disable tokens-only if counting is disabled
  useEffect(() => {
    if (!countTokens) {
      setTokensOnly(false)
    }
  }, [countTokens])

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    
    if (files.length > 0) {
      // Call the onFilesUploaded function with all the options
      onFilesUploaded(
        files, 
        includePatterns, 
        excludePatterns,
        format,
        countTokens,
        tokenEncoding,
        tokensOnly
      );
    }
  }

  const clearAll = () => {
    setFiles([]);
  }

  const toggleAdvanced = () => {
    setShowAdvanced(!showAdvanced);
  }

  return (
    <div className="mt-8">
      <h2 className="text-xl font-semibold mb-4">Upload Files</h2>
      
      <form onSubmit={handleSubmit}>
        <div 
          {...getRootProps()} 
          className={`border-2 border-dashed rounded-lg p-10 text-center cursor-pointer transition-colors ${
            isDragActive ? 'border-blue-500 bg-blue-50' : 'border-gray-300 hover:border-gray-400'
          } ${isProcessing ? 'opacity-50 cursor-not-allowed' : ''}`}
        >
          <input {...getInputProps()} />

          {files.length === 0 ? (
            <div>
              <svg 
                className="mx-auto h-12 w-12 text-gray-400" 
                fill="none" 
                viewBox="0 0 24 24" 
                stroke="currentColor" 
                aria-hidden="true"
              >
                <path 
                  strokeLinecap="round" 
                  strokeLinejoin="round" 
                  strokeWidth={1} 
                  d="M9 13h6m-3-3v6m-9 1V7a2 2 0 012-2h6l2 2h6a2 2 0 012 2v8a2 2 0 01-2 2H5a2 2 0 01-2-2z" 
                />
              </svg>
              <p className="mt-2 text-sm text-gray-600">
                Drag and drop files or directories here, or click to select
              </p>
              <p className="mt-1 text-xs text-gray-500">
                You can upload individual files or entire directories
              </p>
            </div>
          ) : (
            <div>
              <h3 className="text-lg font-medium text-gray-900">{files.length} files selected</h3>
              <ul className="mt-4 max-h-60 overflow-auto text-left">
                {files.map((file, index) => (
                  <li key={index} className="text-sm text-gray-600 flex justify-between items-center py-1">
                    <span className="truncate max-w-md">{file.name}</span>
                    <button
                      type="button"
                      onClick={(e) => {
                        e.stopPropagation();
                        removeFile(index);
                      }}
                      className="ml-2 text-red-500 hover:text-red-700"
                    >
                      Remove
                    </button>
                  </li>
                ))}
              </ul>
              <button
                type="button"
                onClick={(e) => {
                  e.stopPropagation();
                  clearAll();
                }}
                className="mt-4 text-sm text-red-600 hover:text-red-800"
              >
                Clear All
              </button>
            </div>
          )}
        </div>

        <div className="mt-4">
          <button
            type="button"
            onClick={toggleAdvanced}
            className="text-sm font-medium text-blue-600 hover:text-blue-800 focus:outline-none"
          >
            {showAdvanced ? '- Hide Advanced Options' : '+ Show Advanced Options'}
          </button>
        </div>

        {showAdvanced && (
          <div className="mt-4 p-4 bg-gray-50 rounded-md">
            <div className="grid grid-cols-1 md:grid-cols-2 gap-4 mb-4">
              <div>
                <label htmlFor="includePatterns" className="block text-sm font-medium text-gray-700 mb-1">
                  Include Patterns (optional)
                </label>
                <input
                  type="text"
                  id="includePatterns"
                  value={includePatterns}
                  onChange={(e) => setIncludePatterns(e.target.value)}
                  placeholder="*.cpp,*.hpp,src/*"
                  className="w-full px-4 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
                />
                <p className="mt-1 text-xs text-gray-500">
                  Comma-separated glob patterns for files to include.
                </p>
              </div>
              
              <div>
                <label htmlFor="excludePatterns" className="block text-sm font-medium text-gray-700 mb-1">
                  Exclude Patterns (optional)
                </label>
                <input
                  type="text"
                  id="excludePatterns"
                  value={excludePatterns}
                  onChange={(e) => setExcludePatterns(e.target.value)}
                  placeholder="node_modules/*,*.log"
                  className="w-full px-4 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
                />
                <p className="mt-1 text-xs text-gray-500">
                  Comma-separated glob patterns for files to exclude.
                </p>
              </div>
            </div>
            
            <div className="mb-4">
              <label htmlFor="format" className="block text-sm font-medium text-gray-700 mb-1">
                Output Format
              </label>
              <select
                id="format"
                value={format}
                onChange={(e) => setFormat(e.target.value)}
                className="w-full px-4 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
              >
                <option value="plain">Plain Text</option>
                <option value="markdown">Markdown</option>
                <option value="xml">XML</option>
                <option value="claude_xml">Claude XML</option>
              </select>
            </div>

            {/* Token Counting Options */}
            <div className="mb-4 p-4 border border-gray-200 rounded-md">
              <h3 className="text-md font-semibold mb-2 text-gray-700">Token Counting Options</h3>
              
              <div className="flex items-center mb-3">
                <input
                  type="checkbox"
                  id="countTokens"
                  checked={countTokens}
                  onChange={(e) => setCountTokens(e.target.checked)}
                  className="h-4 w-4 text-blue-600 focus:ring-blue-500"
                />
                <label htmlFor="countTokens" className="ml-2 block text-sm text-gray-700">
                  Count tokens in output
                </label>
              </div>
              
              <div className={`mb-3 ${countTokens ? '' : 'opacity-50'}`}>
                <label htmlFor="tokenEncoding" className="block text-sm font-medium text-gray-700 mb-1">
                  Tokenizer Encoding
                </label>
                <select
                  id="tokenEncoding"
                  value={tokenEncoding}
                  onChange={(e) => setTokenEncoding(e.target.value)}
                  disabled={!countTokens}
                  className="w-full px-4 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
                >
                  {TOKENIZER_ENCODINGS.map((encoding) => (
                    <option key={encoding.value} value={encoding.value}>
                      {encoding.label}
                    </option>
                  ))}
                </select>
              </div>
              
              <div className={`flex items-center ${countTokens ? '' : 'opacity-50'}`}>
                <input
                  type="checkbox"
                  id="tokensOnly"
                  checked={tokensOnly}
                  onChange={(e) => setTokensOnly(e.target.checked)}
                  disabled={!countTokens}
                  className="h-4 w-4 text-blue-600 focus:ring-blue-500"
                />
                <label htmlFor="tokensOnly" className="ml-2 block text-sm text-gray-700">
                  Only show token count (faster, no content output)
                </label>
              </div>
            </div>
          </div>
        )}

        <div className="mt-6 flex justify-end">
          <button
            type="submit"
            disabled={isProcessing || files.length === 0}
            className={`px-4 py-2 rounded-md text-white ${
              isProcessing || files.length === 0
                ? 'bg-blue-400 cursor-not-allowed'
                : 'bg-blue-600 hover:bg-blue-700'
            }`}
          >
            {isProcessing ? 'Processing...' : 'Process Files'}
          </button>
        </div>
      </form>
    </div>
  )
}