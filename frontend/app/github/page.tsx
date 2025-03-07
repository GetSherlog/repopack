'use client'

import { useState } from 'react'
import Link from 'next/link'
import { useRouter } from 'next/navigation'
import GitHubForm from '@/components/GitHubForm'
import ProcessingStatus from '@/components/ProcessingStatus'
import ResultDisplay from '@/components/ResultDisplay'

export default function GitHubPage() {
  const router = useRouter()
  const [isProcessing, setIsProcessing] = useState(false)
  const [progress, setProgress] = useState(0)
  const [result, setResult] = useState<string | null>(null)
  const [error, setError] = useState<string | null>(null)
  const [repoUrl, setRepoUrl] = useState<string>('')
  const [tokenCount, setTokenCount] = useState<number | undefined>(undefined)
  const [tokenizer, setTokenizer] = useState<string | undefined>(undefined)

  const handleRepoSubmit = async (
    url: string, 
    includePatterns: string, 
    excludePatterns: string,
    format: string,
    countTokens: boolean,
    tokenEncoding: string,
    tokensOnly: boolean
  ) => {
    setRepoUrl(url)
    setIsProcessing(true)
    setProgress(0)
    setResult(null)
    setError(null)
    setTokenCount(undefined)
    setTokenizer(undefined)

    try {
      // Use our progress simulator
      const interval = setInterval(() => {
        setProgress(prev => {
          if (prev >= 95) {
            clearInterval(interval)
            return 95
          }
          return prev + 5
        })
      }, 300)

      // Import dynamically to prevent server-side rendering issues
      const { processGitRepo } = await import('@/lib/repomix-api')
      
      // Process the GitHub repository with all options
      console.log('Calling processGitRepo with URL:', url);
      const output = await processGitRepo(
        url, 
        format, 
        includePatterns, 
        excludePatterns,
        countTokens,
        tokenEncoding,
        tokensOnly
      );
      console.log('Received API response:', output);
      
      clearInterval(interval)
      setProgress(100)
      
      // Check if there's an error even if success is true
      if (!output.success || output.error) {
        throw new Error(output.error || 'Failed to process repository');
      }
      
      // Set token count information if available
      if (countTokens && output.tokenCount !== undefined) {
        setTokenCount(output.tokenCount);
        setTokenizer(output.tokenizer);
      }
      
      // Make sure we have content (unless tokensOnly is true)
      if (tokensOnly) {
        setResult('Token counting complete. No content was generated as requested.');
      } else if (!output.content && !output.contentInFile) {
        console.warn('No content returned from API');
        // Use placeholder content if none returned
        setResult('Repository processed successfully, but no content was returned.');
      } else if (output.contentInFile && output.contentFilePath) {
        // Content is in a file, show snippet and download link
        const downloadUrl = `${process.env.NEXT_PUBLIC_API_URL || 'http://localhost:9000/api'}/content/${output.contentFilePath}`;
        
        let displayContent = output.contentSnippet || 'Repository processed successfully. The full content is available as a download.';
        
        displayContent += `\n\nFull content available for download: ${downloadUrl}`;
        
        // Add download instruction for LLM use
        displayContent += `\n\nThis file contains a complete repository summary for LLM analysis. Download and view the full content for best results.`;
        
        setResult(displayContent);
        
        // Automatically start download after a short delay
        setTimeout(() => {
          const link = document.createElement('a');
          link.href = downloadUrl;
          link.download = output.contentFilePath || 'repomix-output.txt';
          document.body.appendChild(link);
          link.click();
          document.body.removeChild(link);
        }, 1000);
      } else {
        // Normal content directly in the response
        setResult(output.content || '');
      }
    } catch (err) {
      console.error('Error processing repository:', err);
      setError(err instanceof Error ? err.message : 'An unknown error occurred');
    } finally {
      setIsProcessing(false)
    }
  }

  return (
    <main className="min-h-screen p-8">
      <div className="max-w-5xl mx-auto">
        <div className="mb-8 flex items-center">
          <Link href="/" className="text-primary-600 hover:text-primary-800 flex items-center">
            <svg className="h-5 w-5 mr-2" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M10 19l-7-7m0 0l7-7m-7 7h18" />
            </svg>
            Back to Home
          </Link>
        </div>

        <h1 className="text-3xl font-bold mb-8">Process GitHub Repository</h1>

        {error && (
          <div className="bg-red-50 border border-red-200 text-red-700 px-4 py-3 rounded mb-4">
            <p className="font-medium">Error</p>
            <p>{error}</p>
          </div>
        )}

        {isProcessing && (
          <ProcessingStatus 
            progress={progress}
            message={`Processing repository: ${repoUrl}`}
          />
        )}

        {result && !isProcessing ? (
          <ResultDisplay 
            content={result} 
            tokenCount={tokenCount}
            tokenizer={tokenizer}
          />
        ) : (
          <GitHubForm onRepoSubmit={handleRepoSubmit} isProcessing={isProcessing} />
        )}
      </div>
    </main>
  )
}