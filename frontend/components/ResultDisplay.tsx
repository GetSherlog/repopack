'use client'

import { useState, useRef } from 'react'

interface ResultDisplayProps {
  content: string | null | undefined;
  tokenCount?: number;
  tokenizer?: string;
  onReset?: () => void;
}

export default function ResultDisplay({ content, tokenCount, tokenizer, onReset }: ResultDisplayProps) {
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
    <div className="bg-card border border-border rounded-lg p-6 shadow-sm">
      <div className="flex justify-between items-center mb-4">
        <h2 className="text-2xl font-semibold">Result</h2>
        <div className="flex space-x-2">
          {onReset && (
            <button
              onClick={onReset}
              className="px-4 py-2 bg-secondary hover:bg-secondary/80 text-secondary-foreground rounded-md transition-colors"
            >
              New Request
            </button>
          )}
          <button
            onClick={handleCopy}
            className="px-4 py-2 bg-primary hover:bg-primary/90 text-primary-foreground rounded-md transition-colors"
          >
            {copied ? 'Copied!' : 'Copy'}
          </button>
          <button
            onClick={handleDownload}
            className="px-4 py-2 bg-primary hover:bg-primary/90 text-primary-foreground rounded-md transition-colors"
          >
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