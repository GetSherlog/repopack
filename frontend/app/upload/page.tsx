'use client'

import { useState } from 'react'
import Link from 'next/link'
import { useRouter } from 'next/navigation'
import FileUploader from '@/components/FileUploader'
import ProcessingStatus from '@/components/ProcessingStatus'
import ResultDisplay from '@/components/ResultDisplay'
import FileScoringResults from '@/components/FileScoringResults'
import { FileScoringConfig } from '@/components/FileScoringOptions'

export default function UploadPage() {
  const router = useRouter()
  const [isProcessing, setIsProcessing] = useState(false)
  const [progress, setProgress] = useState(0)
  const [result, setResult] = useState<string | null>(null)
  const [error, setError] = useState<string | null>(null)
  const [tokenCount, setTokenCount] = useState<number | undefined>(undefined)
  const [tokenizer, setTokenizer] = useState<string | undefined>(undefined)
  const [scoringReport, setScoringReport] = useState<any | null>(null)

  const handleFilesUploaded = async (
    files: File[], 
    includePatterns: string, 
    excludePatterns: string,
    format: string,
    countTokens: boolean,
    tokenEncoding: string,
    tokensOnly: boolean,
    fileSelection: string,
    scoringConfig: FileScoringConfig
  ) => {
    setIsProcessing(true)
    setProgress(0)
    setResult(null)
    setError(null)
    setScoringReport(null)
    setTokenCount(undefined)
    setTokenizer(undefined)

    try {
      const formData = new FormData()
      
      // Add all files to the form data
      files.forEach(file => {
        formData.append('files', file, file.name)
      })
      
      // Add other parameters
      if (includePatterns) formData.append('include', includePatterns)
      if (excludePatterns) formData.append('exclude', excludePatterns)
      formData.append('format', format)
      
      // Add token counting options
      if (countTokens) {
        formData.append('count_tokens', 'true')
        formData.append('token_encoding', tokenEncoding)
        if (tokensOnly) {
          formData.append('tokens_only', 'true')
        }
      }
      
      // Add file selection strategy
      if (fileSelection) {
        formData.append('file_selection', fileSelection)
      }
      
      // Add scoring configuration if using scoring strategy
      if (fileSelection === 'scoring') {
        // Add all scoring parameters
        formData.append('root_files_weight', scoringConfig.rootFilesWeight.toString())
        formData.append('top_level_dirs_weight', scoringConfig.topLevelDirsWeight.toString())
        formData.append('entry_points_weight', scoringConfig.entryPointsWeight.toString())
        formData.append('dependency_graph_weight', scoringConfig.dependencyGraphWeight.toString())
        formData.append('source_code_weight', scoringConfig.sourceCodeWeight.toString())
        formData.append('config_files_weight', scoringConfig.configFilesWeight.toString())
        formData.append('documentation_weight', scoringConfig.documentationWeight.toString())
        formData.append('test_files_weight', scoringConfig.testFilesWeight.toString())
        formData.append('recently_modified_weight', scoringConfig.recentlyModifiedWeight.toString())
        formData.append('recent_time_window_days', scoringConfig.recentTimeWindowDays.toString())
        formData.append('file_size_weight', scoringConfig.fileSizeWeight.toString())
        formData.append('large_file_threshold', scoringConfig.largeFileThreshold.toString())
        formData.append('code_density_weight', scoringConfig.codeDensityWeight.toString())
        formData.append('inclusion_threshold', scoringConfig.inclusionThreshold.toString())
        formData.append('use_tree_sitter', scoringConfig.useTreeSitter ? 'true' : 'false')
      }

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
        tokensOnly,
        fileSelection,
        fileSelection === 'scoring' ? scoringConfig : undefined
      )
      
      clearInterval(interval)
      setProgress(100)
      
      // Set token count information if available
      if (countTokens && output.tokenCount !== undefined) {
        setTokenCount(output.tokenCount);
        setTokenizer(output.tokenizer);
      }
      
      // Extract scoring report if available
      if (output.scoring_report) {
        setScoringReport(output.scoring_report);
      }
      
      // Set the result (extract content from API response)
      if (tokensOnly) {
        setResult('Token counting complete. No content was generated as requested.');
      } else if (output.content) {
        setResult(output.content);
      } else {
        setResult(output.summary || 'Processing completed. No content available.');
      }
    } catch (err) {
      console.error('Error processing files:', err);
      setError(err instanceof Error ? err.message : 'An unknown error occurred');
    } finally {
      setIsProcessing(false)
    }
  }

  return (
    <div className="container mx-auto px-4 py-8">
      <div className="mb-8">
        <Link href="/" className="text-blue-600 hover:text-blue-800 flex items-center">
          <svg xmlns="http://www.w3.org/2000/svg" className="h-5 w-5 mr-1" viewBox="0 0 20 20" fill="currentColor">
            <path fillRule="evenodd" d="M9.707 14.707a1 1 0 01-1.414 0l-4-4a1 1 0 010-1.414l4-4a1 1 0 011.414 1.414L7.414 9H15a1 1 0 110 2H7.414l2.293 2.293a1 1 0 010 1.414z" clipRule="evenodd" />
          </svg>
          Back to Home
        </Link>
      </div>
      
      <h1 className="text-3xl font-bold mb-8">Upload Repository</h1>
      
      <div className="grid grid-cols-1 md:grid-cols-2 gap-8">
        <div>
          <FileUploader 
            onFilesUploaded={handleFilesUploaded} 
            isProcessing={isProcessing} 
          />
        </div>
        
        <div>
          <div className="bg-white shadow-md rounded-lg overflow-hidden">
            <div className="p-6">
              <h2 className="text-xl font-semibold mb-4">Results</h2>
              
              {isProcessing && (
                <ProcessingStatus progress={progress} />
              )}
              
              {error && (
                <div className="bg-red-100 border border-red-400 text-red-700 px-4 py-3 rounded relative mb-4">
                  <strong className="font-bold">Error: </strong>
                  <span className="block sm:inline">{error}</span>
                </div>
              )}
              
              {/* Display scoring results if available */}
              {scoringReport && (
                <div className="mb-6">
                  <h3 className="text-lg font-semibold mb-2">File Scoring Results</h3>
                  <FileScoringResults scoringReport={scoringReport} />
                </div>
              )}
              
              {tokenCount !== undefined && (
                <div className="bg-blue-50 p-4 rounded-md mb-4">
                  <p className="text-blue-800">
                    <span className="font-bold">Token count:</span> {tokenCount.toLocaleString()} 
                    {tokenizer && <span className="text-blue-600 text-sm ml-2">({tokenizer})</span>}
                  </p>
                </div>
              )}
              
              {result && (
                <ResultDisplay content={result} />
              )}
            </div>
          </div>
        </div>
      </div>
    </div>
  )
}