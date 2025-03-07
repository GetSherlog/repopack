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

  const handleRepoSubmit = async (url: string, includePatterns: string, excludePatterns: string) => {
    setRepoUrl(url)
    setIsProcessing(true)
    setProgress(0)
    setResult(null)
    setError(null)

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
      
      // Process the GitHub repository with include/exclude patterns
      console.log('Calling processGitRepo with URL:', url);
      const output = await processGitRepo(url, 'plain', includePatterns, excludePatterns);
      console.log('Received API response:', output);
      
      clearInterval(interval)
      setProgress(100)
      
      // Check if there's an error even if success is true
      if (!output.success || output.error) {
        throw new Error(output.error || 'Failed to process repository');
      }
      
      // Make sure we have content
      if (!output.content && !output.contentInFile) {
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
            <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className="mr-2">
              <path d="M19 12H5M12 19l-7-7 7-7" />
            </svg>
            Back to home
          </Link>
          <h1 className="text-3xl font-bold ml-auto mr-auto">GitHub Repository</h1>
          <div className="w-24"></div> {/* Spacer for alignment */}
        </div>

        {!result && (
          <GitHubForm 
            onRepoSubmit={handleRepoSubmit}
            isProcessing={isProcessing}
          />
        )}

        {isProcessing && (
          <ProcessingStatus progress={progress} />
        )}

        {error && (
          <div className="mt-8 p-4 bg-red-50 border border-red-200 rounded-lg text-red-700">
            <h3 className="font-semibold mb-2">Error</h3>
            <p>{error}</p>
          </div>
        )}

        {result && (
          <>
            <div className="mb-4 p-4 bg-blue-50 border border-blue-200 rounded-lg">
              <h3 className="font-semibold mb-2">Repository Processed</h3>
              <p className="break-all">{repoUrl}</p>
            </div>
            <ResultDisplay result={result} />
          </>
        )}
      </div>
    </main>
  )
}