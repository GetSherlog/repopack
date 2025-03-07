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

  const handleFilesUploaded = async (files: File[], includePatterns: string, excludePatterns: string) => {
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
      const { processFiles } = await import('@/lib/repomix-api')
      
      // Process the files with include/exclude patterns
      const output = await processFiles(files, 'plain', includePatterns, excludePatterns)
      
      clearInterval(interval)
      setProgress(100)
      
      // Set the result (extract content from API response)
      setResult(output.content || null)
    } catch (err) {
      setError(err instanceof Error ? err.message : 'An unknown error occurred')
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
          <h1 className="text-3xl font-bold ml-auto mr-auto">Upload Files</h1>
          <div className="w-24"></div> {/* Spacer for alignment */}
        </div>

        {!result && (
          <FileUploader 
            onFilesUploaded={handleFilesUploaded} 
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
          <ResultDisplay result={result} />
        )}
      </div>
    </main>
  )
}