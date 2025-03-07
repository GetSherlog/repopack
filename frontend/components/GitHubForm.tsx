'use client'

import { useState, useEffect } from 'react'

interface GitHubFormProps {
  onRepoSubmit: (url: string) => void;
  isProcessing: boolean;
}

export default function GitHubForm({ onRepoSubmit, isProcessing }: GitHubFormProps) {
  const [repoUrl, setRepoUrl] = useState('')
  const [githubToken, setGithubToken] = useState('')
  const [error, setError] = useState<string | null>(null)

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

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault()
    
    if (!repoUrl.trim()) {
      setError('Please enter a GitHub repository URL')
      return
    }

    if (!validateGitHubUrl(repoUrl)) {
      setError('Please enter a valid GitHub repository URL (e.g., https://github.com/username/repo)')
      return
    }

    setError(null)
    onRepoSubmit(repoUrl)
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