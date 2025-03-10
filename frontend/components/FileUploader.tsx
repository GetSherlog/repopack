'use client'

import { useCallback, useState, useEffect } from 'react'
import { useDropzone } from 'react-dropzone'
import SummarizationOptions, { SummarizationOptions as SummarizationOptionsType, defaultSummarizationOptions } from './SummarizationOptions'
import FileScoringOptions, { FileScoringConfig, defaultFileScoringConfig } from './FileScoringOptions'
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
    tokensOnly: boolean,
    fileSelection: string,
    scoringConfig: FileScoringConfig
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
  const [fileSelection, setFileSelection] = useState<string>('all')
  const [scoringConfig, setScoringConfig] = useState<FileScoringConfig>(defaultFileScoringConfig)

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
    e.preventDefault()
    
    if (files.length === 0) {
      alert('Please select files to upload')
      return
    }
    
    // Call the parent component's handler with all options
    onFilesUploaded(
      files,
      includePatterns,
      excludePatterns,
      format,
      countTokens,
      tokenEncoding,
      tokensOnly,
      fileSelection,
      scoringConfig
    )
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
      
      <form onSubmit={handleSubmit} className="space-y-4">
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
          <div className="space-y-4 pt-4 border-t">
            {/* Token counting options */}
            <div className="space-y-2">
              <div className="flex items-start">
                <div className="h-5 flex items-center">
                  <input
                    id="countTokens"
                    name="countTokens"
                    type="checkbox"
                    className="focus:ring-indigo-500 h-4 w-4 text-indigo-600 border-gray-300 rounded"
                    checked={countTokens}
                    onChange={(e) => setCountTokens(e.target.checked)}
                    disabled={isProcessing}
                  />
                </div>
                <div className="ml-3 text-sm">
                  <label htmlFor="countTokens" className="font-medium text-gray-700">Count Tokens</label>
                  <p className="text-gray-500">Include token count in the output</p>
                </div>
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
            
            {/* File Scoring Options */}
            <FileScoringOptions 
              config={scoringConfig} 
              onChange={setScoringConfig} 
            />
            
            {/* Summarization Options */}
            <div className="pt-4">
              <h4 className="text-sm font-medium text-gray-900 mb-2">Summarization Options</h4>
              <SummarizationOptions 
                options={summarizationOptions} 
                onChange={setSummarizationOptions} 
              />
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

      {/* File list */}
      {files.length > 0 && (
        <div className="mt-4">
          <h3 className="text-sm font-medium text-gray-700">Selected Files ({files.length})</h3>
          <ul className="mt-2 border border-gray-200 rounded-md divide-y divide-gray-200 max-h-40 overflow-auto">
            {files.map((file, index) => (
              <li key={index} className="pl-3 pr-4 py-3 flex items-center justify-between text-sm">
                <div className="w-0 flex-1 flex items-center">
                  <span className="ml-2 flex-1 w-0 truncate">{file.name}</span>
                </div>
                <div className="ml-4 flex-shrink-0">
                  <button
                    type="button"
                    onClick={() => removeFile(index)}
                    className="font-medium text-indigo-600 hover:text-indigo-500"
                    disabled={isProcessing}
                  >
                    Remove
                  </button>
                </div>
              </li>
            ))}
          </ul>
        </div>
      )}
      
      {/* Format selection */}
      <div>
        <label htmlFor="format" className="block text-sm font-medium text-gray-700">Output Format</label>
        <select
          id="format"
          name="format"
          className="mt-1 block w-full pl-3 pr-10 py-2 text-base border-gray-300 focus:outline-none focus:ring-indigo-500 focus:border-indigo-500 sm:text-sm rounded-md"
          value={format}
          onChange={(e) => setFormat(e.target.value)}
          disabled={isProcessing}
        >
          <option value="plain">Plain Text</option>
          <option value="markdown">Markdown</option>
          <option value="xml">XML</option>
          <option value="claude_xml">Claude XML</option>
        </select>
      </div>
      
      {/* File selection strategy */}
      <div>
        <label htmlFor="fileSelection" className="block text-sm font-medium text-gray-700">File Selection Strategy</label>
        <select
          id="fileSelection"
          name="fileSelection"
          className="mt-1 block w-full pl-3 pr-10 py-2 text-base border-gray-300 focus:outline-none focus:ring-indigo-500 focus:border-indigo-500 sm:text-sm rounded-md"
          value={fileSelection}
          onChange={(e) => setFileSelection(e.target.value)}
          disabled={isProcessing}
        >
          <option value="all">All Files (Basic Filtering)</option>
          <option value="scoring">Smart Scoring System</option>
        </select>
        <p className="mt-1 text-sm text-gray-500">
          {fileSelection === 'all' 
            ? 'Include all files, filtered only by ignore patterns' 
            : 'Use a weighted scoring system to select the most relevant files'}
        </p>
      </div>
      
      {/* Include/exclude patterns */}
    </div>
  )
}