'use client'

import { useState, useEffect } from 'react'
import SummarizationOptions, { SummarizationOptions as SummarizationOptionsType, defaultSummarizationOptions } from './SummarizationOptions'
import FileScoringOptions, { FileScoringConfig, defaultFileScoringConfig } from './FileScoringOptions'
import { TOKENIZER_ENCODINGS } from '../lib/repomix-api'

interface GitHubFormProps {
  onRepoSubmit: (
    url: string, 
    includePatterns: string, 
    excludePatterns: string, 
    format: string,
    countTokens: boolean,
    tokenEncoding: string,
    tokensOnly: boolean,
    summarizationOptions: SummarizationOptionsType,
    showVerbose: boolean,
    showTiming: boolean,
    fileSelectionStrategy: string,
    fileScoringConfig: FileScoringConfig
  ) => void;
  isProcessing: boolean;
}

export default function GitHubForm({ onRepoSubmit, isProcessing }: GitHubFormProps) {
  const [repoUrl, setRepoUrl] = useState('')
  const [githubToken, setGithubToken] = useState('')
  const [includePatterns, setIncludePatterns] = useState('')
  const [excludePatterns, setExcludePatterns] = useState('')
  const [showAdvanced, setShowAdvanced] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const [summarizationOptions, setSummarizationOptions] = useState<SummarizationOptionsType>(defaultSummarizationOptions)
  const [showVerbose, setShowVerbose] = useState(false)
  const [showTiming, setShowTiming] = useState(false)
  const [format, setFormat] = useState('plain')
  const [countTokens, setCountTokens] = useState(false)
  const [tokenEncoding, setTokenEncoding] = useState('cl100k_base')
  const [tokensOnly, setTokensOnly] = useState(false)
  const [fileSelectionStrategy, setFileSelectionStrategy] = useState('all')
  const [fileScoringConfig, setFileScoringConfig] = useState<FileScoringConfig>(defaultFileScoringConfig)

  // Load token from localStorage if available
  useEffect(() => {
    const savedToken = localStorage.getItem('githubToken')
    if (savedToken) {
      setGithubToken(savedToken)
    }
  }, [])

  // Save token to localStorage when it changes
  useEffect(() => {
    if (githubToken) {
      localStorage.setItem('githubToken', githubToken)
    }
  }, [githubToken])

  const validateGitHubUrl = (url: string): boolean => {
    // Simple validation for GitHub URLs
    // This can be expanded for more robust validation
    const githubPattern = /^https?:\/\/(www\.)?github\.com\/[\w.-]+\/[\w.-]+\/?$/
    return githubPattern.test(url)
  }

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault()
    
    if (!repoUrl.trim()) {
      setError('Please enter a GitHub repository URL')
      return
    }
    
    // Validate URL format
    if (!repoUrl.match(/^https:\/\/github\.com\/[^/]+\/[^/]+/)) {
      setError('Please enter a valid GitHub repository URL (https://github.com/owner/repo)')
      return
    }

    setError(null)
    try {
      // Call the handler with all options
      onRepoSubmit(
        repoUrl, 
        includePatterns, 
        excludePatterns, 
        format,
        countTokens,
        tokenEncoding,
        tokensOnly,
        summarizationOptions,
        showVerbose,
        showTiming,
        fileSelectionStrategy,
        fileScoringConfig
      )
    } catch (err: any) {
      setError(`Error: ${err?.message || 'An unknown error occurred'}`);
    }
  }

  const toggleAdvanced = () => {
    setShowAdvanced(!showAdvanced)
  }

  // Disable tokens-only if counting is disabled
  useEffect(() => {
    if (!countTokens) {
      setTokensOnly(false)
    }
  }, [countTokens])

  return (
    <div className="mt-8">
      <h2 className="text-xl font-semibold mb-4">Process GitHub Repository</h2>
      
      {error && (
        <div className="bg-red-50 border border-red-200 text-red-700 px-4 py-3 rounded mb-4">
          {error}
        </div>
      )}
      
      <form onSubmit={handleSubmit}>
        <div className="mb-4">
          <label htmlFor="repoUrl" className="block text-sm font-medium text-gray-700 mb-1">
            GitHub Repository URL
          </label>
          <input
            type="text"
            id="repoUrl"
            value={repoUrl}
            onChange={(e) => setRepoUrl(e.target.value)}
            placeholder="https://github.com/username/repository"
            className="w-full px-4 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
            required
          />
        </div>
        
        <div className="mb-4">
          <label htmlFor="githubToken" className="block text-sm font-medium text-gray-700 mb-1">
            GitHub Token (optional, for private repositories)
          </label>
          <input
            type="password"
            id="githubToken"
            value={githubToken}
            onChange={(e) => setGithubToken(e.target.value)}
            placeholder="Your GitHub personal access token"
            className="w-full px-4 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
          />
          <p className="mt-1 text-xs text-gray-500">
            Token will be stored in your browser for future use.
          </p>
        </div>
        
        <div className="mb-4">
          <button
            type="button"
            onClick={toggleAdvanced}
            className="text-sm font-medium text-blue-600 hover:text-blue-800 focus:outline-none"
          >
            {showAdvanced ? '- Hide Advanced Options' : '+ Show Advanced Options'}
          </button>
        </div>
        
        {showAdvanced && (
          <div className="mb-6 p-4 bg-gray-50 rounded-md">
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

            {/* Processing Options */}
            <div className="mb-4 p-4 border border-gray-200 rounded-md">
              <h3 className="text-md font-semibold mb-2 text-gray-700">Processing Options</h3>
              
              <div className="flex items-center mb-3">
                <input
                  type="checkbox"
                  id="showVerbose"
                  checked={showVerbose}
                  onChange={(e) => setShowVerbose(e.target.checked)}
                  className="h-4 w-4 text-blue-600 focus:ring-blue-500"
                />
                <label htmlFor="showVerbose" className="ml-2 block text-sm text-gray-700">
                  Verbose output
                </label>
              </div>
              
              <div className="flex items-center mb-3">
                <input
                  type="checkbox"
                  id="showTiming"
                  checked={showTiming}
                  onChange={(e) => setShowTiming(e.target.checked)}
                  className="h-4 w-4 text-blue-600 focus:ring-blue-500"
                />
                <label htmlFor="showTiming" className="ml-2 block text-sm text-gray-700">
                  Show timing information
                </label>
              </div>
              
              <div className="mb-3">
                <label htmlFor="fileSelectionStrategy" className="block text-sm font-medium text-gray-700 mb-1">
                  File Selection Strategy
                </label>
                <select
                  id="fileSelectionStrategy"
                  value={fileSelectionStrategy}
                  onChange={(e) => setFileSelectionStrategy(e.target.value)}
                  className="w-full px-4 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
                >
                  <option value="all">All Files</option>
                  <option value="scoring">Smart File Selection (Scoring)</option>
                </select>
                <p className="mt-1 text-xs text-gray-500">
                  Smart selection uses scoring to prioritize the most important files.
                </p>
              </div>
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
                <label htmlFor="tokensOnly" className="ml-2 text-sm text-gray-700">
                  Only show token count (faster, no content output)
                </label>
              </div>
            </div>
            
            {/* Smart Summarization Options */}
            <div className="mb-4 p-4 border border-gray-200 rounded-md">
              <SummarizationOptions 
                options={summarizationOptions} 
                onChange={setSummarizationOptions} 
              />
            </div>
            
            {/* File Scoring Options - only show when scoring strategy is selected */}
            {fileSelectionStrategy === 'scoring' && (
              <div className="mb-4 p-4 border border-gray-200 rounded-md">
                <FileScoringOptions 
                  config={fileScoringConfig} 
                  onChange={setFileScoringConfig} 
                />
              </div>
            )}
          </div>
        )}
        
        <div className="mt-6">
          <button
            type="submit"
            disabled={isProcessing}
            className={`w-full py-2 px-4 rounded-md text-white font-medium ${
              isProcessing
                ? 'bg-blue-400 cursor-not-allowed'
                : 'bg-blue-600 hover:bg-blue-700'
            }`}
          >
            {isProcessing ? 'Processing...' : 'Process Repository'}
          </button>
        </div>
      </form>
    </div>
  )
}