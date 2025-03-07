'use client'

import { useState } from 'react'
import Link from 'next/link'
import { useRouter } from 'next/navigation'
import FileUploader from '@/components/FileUploader'
import ProcessingStatus from '@/components/ProcessingStatus'
import ResultDisplay from '@/components/ResultDisplay'

export default function UploadPage() {
  const router = useRouter()
  const [isProcessing, setIsProcessing] = useState(false)
  const [progress, setProgress] = useState(0)
  const [result, setResult] = useState<string | null>(null)
  const [error, setError] = useState<string | null>(null)
  const [tokenCount, setTokenCount] = useState<number | undefined>(undefined)
  const [tokenizer, setTokenizer] = useState<string | undefined>(undefined)

  const handleFilesUploaded = async (
    files: File[], 
    includePatterns: string, 
    excludePatterns: string,
    format: string,
    countTokens: boolean,
    tokenEncoding: string,
    tokensOnly: boolean
  ) => {
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
      const { processFiles } = await import('@/lib/repomix-api')
      
      // Process the files with all options
      const output = await processFiles(
        files, 
        format, 
        includePatterns, 
        excludePatterns,
        countTokens,
        tokenEncoding,
        tokensOnly
      )
      
      clearInterval(interval)
      setProgress(100)
      
      // Set token count information if available
      if (countTokens && output.tokenCount !== undefined) {
        setTokenCount(output.tokenCount);
        setTokenizer(output.tokenizer);
      }
      
      // Set the result (extract content from API response)
      if (tokensOnly) {
        setResult('Token counting complete. No content was generated as requested.');
      } else {
        setResult(output.content || null)
      }
    } catch (err) {
      console.error('Error processing files:', err);
      setError(err instanceof Error ? err.message : 'An unknown error occurred');
    } finally {
      setIsProcessing(false)
    }
  }

  return (
    <main className="min-h-screen p-8">
      <div className="max-w-5xl mx-auto">
        <div className="mb-8 flex items-center">
          <Link href="/" className="text-blue-600 hover:text-blue-800 flex items-center">
            <svg className="h-5 w-5 mr-2" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M10 19l-7-7m0 0l7-7m-7 7h18" />
            </svg>
            Back to Home
          </Link>
        </div>

        <h1 className="text-3xl font-bold mb-8">Upload Files</h1>

        {error && (
          <div className="bg-red-50 border border-red-200 text-red-700 px-4 py-3 rounded mb-4">
            <p className="font-medium">Error</p>
            <p>{error}</p>
          </div>
        )}

        {isProcessing && (
          <ProcessingStatus progress={progress} />
        )}

        {result && !isProcessing ? (
          <ResultDisplay 
            result={result} 
            tokenCount={tokenCount}
            tokenizer={tokenizer}
          />
        ) : (
          <FileUploader 
            onFilesUploaded={handleFilesUploaded}
            isProcessing={isProcessing} 
          />
        )}
      </div>
    </main>
  )
}