'use client'

import { useState } from 'react'
import Link from 'next/link'
import { useRouter } from 'next/navigation'
import GitHubForm from '@/components/GitHubForm'
import ProcessingStatus from '@/components/ProcessingStatus'
import ResultDisplay from '@/components/ResultDisplay'
import ProcessingProgress from '@/components/ProcessingProgress'
import { processGitRepo, RepomixApiResponse } from '@/lib/repomix-api'

export default function GitHubPage() {
  const router = useRouter()
  const [isProcessing, setIsProcessing] = useState(false)
  const [progress, setProgress] = useState(0)
  const [result, setResult] = useState<string | null>(null)
  const [error, setError] = useState<string | null>(null)
  const [repoUrl, setRepoUrl] = useState<string>('')
  const [tokenCount, setTokenCount] = useState<number | undefined>(undefined)
  const [tokenizer, setTokenizer] = useState<string | undefined>(undefined)
  const [jobId, setJobId] = useState<string | null>(null)

  const handleRepoSubmit = async (
    url: string, 
    includePatterns: string, 
    excludePatterns: string,
    format: string,
    countTokens: boolean,
    tokenEncoding: string,
    tokensOnly: boolean,
    summarizationOptions: any,
    showVerbose: boolean,
    showTiming: boolean,
    fileSelectionStrategy: string,
    fileScoringConfig: any
  ) => {
    setRepoUrl(url)
    setIsProcessing(true)
    setProgress(0)
    setResult(null)
    setError(null)
    setTokenCount(undefined)
    setTokenizer(undefined)
    setJobId(null)

    try {
      // Process the repository
      const response = await processGitRepo(
        url,
        format,
        includePatterns,
        excludePatterns,
        countTokens,
        tokenEncoding,
        tokensOnly,
        summarizationOptions,
        showVerbose,
        showTiming,
        fileSelectionStrategy,
        fileScoringConfig
      );

      // Check if we have a job ID for progress tracking
      if (response.jobId) {
        setJobId(response.jobId);
      } else {
        // If no job ID, use the old progress simulator
        const interval = setInterval(() => {
          setProgress(prev => {
            if (prev >= 95) {
              clearInterval(interval)
              return 95
            }
            return prev + 5
          })
        }, 500)
      }

      // Handle the response
      if (response.success) {
        setProgress(100)
        setResult(response.content || response.contentSnippet || 'Processing completed successfully')
        
        if (response.tokenCount !== undefined) {
          setTokenCount(response.tokenCount)
          setTokenizer(response.tokenizer)
        }
      } else {
        throw new Error(response.error || 'Unknown error occurred')
      }
    } catch (err: any) {
      setError(err.message || 'Failed to process repository')
    } finally {
      setIsProcessing(false)
    }
  }

  const handleProgressComplete = (success: boolean) => {
    // This is called when the progress component reports completion
    // We can use this to update the UI if needed
    console.log('Processing completed with success:', success);
  }

  return (
    <main className="min-h-screen p-8 flex flex-col items-center bg-background">
      <div className="max-w-5xl w-full">
        <Link href="/" className="text-primary hover:underline mb-8 inline-flex items-center">
          <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className="mr-2">
            <path d="M19 12H5M12 19l-7-7 7-7"/>
          </svg>
          Back to Home
        </Link>

        <h1 className="text-4xl font-bold mb-8">Process GitHub Repository</h1>

        {!isProcessing && !result && !error && (
          <GitHubForm onSubmit={handleRepoSubmit} />
        )}

        {isProcessing && !error && (
          <>
            {jobId ? (
              <ProcessingProgress 
                jobId={jobId} 
                onComplete={handleProgressComplete} 
              />
            ) : (
              <ProcessingStatus 
                progress={progress} 
                message={`Processing repository: ${repoUrl}`} 
              />
            )}
          </>
        )}

        {error && (
          <div className="bg-red-50 border border-red-200 text-red-700 p-4 rounded-md mb-8">
            <h3 className="text-lg font-semibold mb-2">Error</h3>
            <p>{error}</p>
            <button 
              onClick={() => {
                setError(null)
                setIsProcessing(false)
              }}
              className="mt-4 px-4 py-2 bg-red-100 hover:bg-red-200 text-red-700 rounded-md transition-colors"
            >
              Try Again
            </button>
          </div>
        )}

        {result && (
          <ResultDisplay 
            content={result} 
            tokenCount={tokenCount} 
            tokenizer={tokenizer} 
            onReset={() => {
              setResult(null)
              setTokenCount(undefined)
              setTokenizer(undefined)
            }} 
          />
        )}
      </div>
    </main>
  )
}