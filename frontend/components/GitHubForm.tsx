'use client'

import { useState, useEffect } from 'react'
import SummarizationOptions, { SummarizationOptions as SummarizationOptionsType, defaultSummarizationOptions } from './SummarizationOptions'

interface GitHubFormProps {
  onRepoSubmit: (url: string, includePatterns: string, excludePatterns: string) => void;
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
      const response = await fetch('/api/process_repo', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          url: repoUrl,
          include_patterns: includePatterns,
          exclude_patterns: excludePatterns,
          token: githubToken,
          format: format,
          verbose: showVerbose,
          timing: showTiming,
          summarization: summarizationOptions.enabled ? summarizationOptions : undefined
        }),
      })

      if (response.ok) {
        onRepoSubmit(repoUrl, includePatterns, excludePatterns)
      } else {
        setError('Failed to process repository')
      }
    } catch (error) {
      setError('An error occurred')
    }
  }

  const toggleAdvanced = () => {
    setShowAdvanced(prev => !prev)
  }

  return (
    <div className="mt-8">
      <div className="bg-card p-8 rounded-lg border border-border shadow-sm">
        <h2 className="text-xl font-semibold mb-4">Enter GitHub Repository URL</h2>
        
        <form onSubmit={handleSubmit}>
          <div className="mb-4">
            <label htmlFor="repoUrl" className="block text-sm font-medium text-foreground mb-1">
              Repository URL
            </label>
            <input
              type="text"
              id="repoUrl"
              value={repoUrl}
              onChange={(e) => setRepoUrl(e.target.value)}
              placeholder="https://github.com/username/repository"
              className="w-full px-4 py-2 border border-input rounded-md focus:outline-none focus:ring-2 focus:ring-ring focus:border-transparent bg-background text-foreground"
              disabled={isProcessing}
            />
            {error && (
              <p className="mt-1 text-sm text-destructive">{error}</p>
            )}
            <p className="mt-1 text-sm text-muted-foreground">
              Enter the full URL to a GitHub repository
            </p>
          </div>

          <div className="mb-4">
            <label htmlFor="githubToken" className="block text-sm font-medium text-foreground mb-1">
              GitHub Token (Optional)
            </label>
            <input
              type="password"
              id="githubToken"
              value={githubToken}
              onChange={(e) => setGithubToken(e.target.value)}
              placeholder="ghp_xxxxxxxxx"
              className="w-full px-4 py-2 border border-input rounded-md focus:outline-none focus:ring-2 focus:ring-ring focus:border-transparent bg-background text-foreground"
              disabled={isProcessing}
            />
            <p className="mt-1 text-sm text-muted-foreground">
              Provide a GitHub Personal Access Token to avoid rate limiting. Your token will be stored locally and never sent to our servers.
            </p>
          </div>
          
          <button
            type="button"
            onClick={toggleAdvanced}
            className="text-sm text-primary hover:text-primary/90 flex items-center mb-4"
          >
            {showAdvanced ? (
              <>
                <svg xmlns="http://www.w3.org/2000/svg" className="h-4 w-4 mr-1" viewBox="0 0 20 20" fill="currentColor">
                  <path fillRule="evenodd" d="M5.293 7.293a1 1 0 011.414 0L10 10.586l3.293-3.293a1 1 0 111.414 1.414l-4 4a1 1 0 01-1.414 0l-4-4a1 1 0 010-1.414z" clipRule="evenodd" />
                </svg>
                Hide advanced options
              </>
            ) : (
              <>
                <svg xmlns="http://www.w3.org/2000/svg" className="h-4 w-4 mr-1" viewBox="0 0 20 20" fill="currentColor">
                  <path fillRule="evenodd" d="M7.293 14.707a1 1 0 010-1.414L10.586 10 7.293 6.707a1 1 0 011.414-1.414l4 4a1 1 0 010 1.414l-4 4a1 1 0 01-1.414 0z" clipRule="evenodd" />
                </svg>
                Show advanced options
              </>
            )}
          </button>
          
          {showAdvanced && (
            <div className="mb-6 space-y-4 border border-gray-200 rounded-lg p-4 bg-gray-50">
              <div>
                <label htmlFor="format" className="block text-sm font-medium text-gray-700 mb-1">
                  Output Format
                </label>
                <select
                  id="format"
                  value={format}
                  onChange={(e) => setFormat(e.target.value)}
                  className="block w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-primary-500 focus:border-primary-500 sm:text-sm"
                  disabled={isProcessing}
                >
                  <option value="plain">Plain Text</option>
                  <option value="markdown">Markdown</option>
                  <option value="xml">XML</option>
                  <option value="claude_xml">Claude XML (Optimized for Anthropic Claude)</option>
                </select>
                <p className="mt-1 text-xs text-gray-500">
                  Select the output format for the repository content
                </p>
              </div>
              
              <div>
                <label htmlFor="include-patterns" className="block text-sm font-medium text-gray-700 mb-1">
                  Include patterns (comma-separated)
                </label>
                <input
                  id="include-patterns"
                  type="text"
                  value={includePatterns}
                  onChange={(e) => setIncludePatterns(e.target.value)}
                  placeholder="*.rs,*.toml"
                  className="block w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-primary-500 focus:border-primary-500 sm:text-sm"
                  disabled={isProcessing}
                />
                <p className="mt-1 text-xs text-gray-500">
                  Only files matching these patterns will be processed
                </p>
              </div>
              
              <div>
                <label htmlFor="exclude-patterns" className="block text-sm font-medium text-gray-700 mb-1">
                  Exclude patterns (comma-separated)
                </label>
                <input
                  id="exclude-patterns"
                  type="text"
                  value={excludePatterns}
                  onChange={(e) => setExcludePatterns(e.target.value)}
                  placeholder="*.txt,*.md"
                  className="block w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-primary-500 focus:border-primary-500 sm:text-sm"
                  disabled={isProcessing}
                />
                <p className="mt-1 text-xs text-gray-500">
                  Files matching these patterns will be excluded
                </p>
              </div>
            </div>
          )}

          {/* Summarization Options */}
          <SummarizationOptions 
            options={summarizationOptions}
            onChange={setSummarizationOptions}
          />

          <div className="mt-6 flex justify-end">
            <button
              type="submit"
              disabled={isProcessing}
              className={`px-6 py-2 rounded-md font-medium ${
                isProcessing
                  ? 'bg-muted text-muted-foreground cursor-not-allowed'
                  : 'bg-primary text-primary-foreground hover:bg-primary/90'
              }`}
            >
              {isProcessing ? 'Processing...' : 'Process Repository'}
            </button>
          </div>
        </form>

        <div className="mt-6">
          <h3 className="text-sm font-medium text-foreground mb-2">Examples:</h3>
          <ul className="space-y-1 text-sm text-muted-foreground">
            <li>• https://github.com/facebook/react</li>
            <li>• https://github.com/tensorflow/tensorflow</li>
            <li>• https://github.com/microsoft/vscode</li>
          </ul>
        </div>
      </div>
    </div>
  )
}