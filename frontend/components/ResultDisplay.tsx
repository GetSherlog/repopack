'use client'

import { useState, useRef } from 'react'

interface ResultDisplayProps {
  content: string | null | undefined;
  tokenCount?: number;
  tokenizer?: string;
}

export default function ResultDisplay({ content, tokenCount, tokenizer }: ResultDisplayProps) {
  const [copied, setCopied] = useState(false)
  const textAreaRef = useRef<HTMLTextAreaElement>(null)
  
  // Ensure we have a string value for the result
  const displayResult = content || 'No content available';

  const handleCopy = () => {
    if (textAreaRef.current) {
      textAreaRef.current.select()
      document.execCommand('copy')
      setCopied(true)
      
      setTimeout(() => {
        setCopied(false)
      }, 2000)
    }
  }

  const handleDownload = () => {
    const blob = new Blob([displayResult], { type: 'text/plain' })
    const url = URL.createObjectURL(blob)
    const a = document.createElement('a')
    a.href = url
    a.download = 'repomix-output.txt'
    document.body.appendChild(a)
    a.click()
    document.body.removeChild(a)
    URL.revokeObjectURL(url)
  }

  return (
    <div className="mt-8">
      <div className="flex justify-between items-center mb-4">
        <h2 className="text-xl font-semibold">Processing Result</h2>
        <div className="space-x-2">
          <button
            onClick={handleCopy}
            className="px-4 py-2 rounded-md bg-gray-100 hover:bg-gray-200 text-gray-700 text-sm font-medium flex items-center"
          >
            <svg className="h-4 w-4 mr-1" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M8 16H6a2 2 0 01-2-2V6a2 2 0 012-2h8a2 2 0 012 2v2m-6 12h8a2 2 0 002-2v-8a2 2 0 00-2-2h-8a2 2 0 00-2 2v8a2 2 0 002 2z" />
            </svg>
            {copied ? 'Copied!' : 'Copy'}
          </button>
          <button
            onClick={handleDownload}
            className="px-4 py-2 rounded-md bg-gray-100 hover:bg-gray-200 text-gray-700 text-sm font-medium flex items-center"
          >
            <svg className="h-4 w-4 mr-1" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-4l-4 4m0 0l-4-4m4 4V4" />
            </svg>
            Download
          </button>
        </div>
      </div>

      {/* Token Count Display */}
      {tokenCount !== undefined && (
        <div className="mb-4 p-4 bg-blue-50 border border-blue-200 rounded-md">
          <div className="flex items-center">
            <svg className="h-5 w-5 text-blue-500 mr-2" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M13 10V3L4 14h7v7l9-11h-7z" />
            </svg>
            <div>
              <span className="font-medium">Token Count: </span>
              <span className="font-bold">{tokenCount.toLocaleString()}</span>
              {tokenizer && (
                <span className="text-sm text-gray-600 ml-2">
                  using {tokenizer} tokenizer
                </span>
              )}
            </div>
          </div>
        </div>
      )}
      
      <div className="border border-gray-200 rounded-md overflow-hidden">
        <textarea
          ref={textAreaRef}
          className="w-full h-96 p-4 font-mono text-sm resize-none focus:outline-none"
          value={displayResult}
          readOnly
        />
      </div>
    </div>
  )
}