'use client'

import { useCallback, useState } from 'react'
import { useDropzone } from 'react-dropzone'

// Add type declaration for HTML input attributes
declare module 'react' {
  interface InputHTMLAttributes<T> extends HTMLAttributes<T> {
    directory?: string;
    webkitdirectory?: string;
  }
}

interface FileUploaderProps {
  onFilesUploaded: (files: File[]) => void;
  isProcessing: boolean;
}

export default function FileUploader({ onFilesUploaded, isProcessing }: FileUploaderProps) {
  const [files, setFiles] = useState<File[]>([])

  const onDrop = useCallback((acceptedFiles: File[]) => {
    // Handle folder uploads by recursively traversing the FileSystemEntry structure
    // Note: In a real implementation, we'd use the FileSystemAPI, but for simplicity
    // we're just collecting the flat list of files here
    setFiles(prev => [...prev, ...acceptedFiles])
  }, [])

  const { getRootProps, getInputProps, isDragActive } = useDropzone({
    onDrop,
    disabled: isProcessing,
    multiple: true,
    noClick: files.length > 0
  })

  const removeFile = (index: number) => {
    setFiles(prev => prev.filter((_, i) => i !== index))
  }

  const handleSubmit = () => {
    if (files.length > 0) {
      onFilesUploaded(files)
    }
  }

  const clearAll = () => {
    setFiles([])
  }

  return (
    <div className="mt-8">
      <div 
        {...getRootProps()} 
        className={`p-8 border-2 border-dashed rounded-lg cursor-pointer transition-colors ${
          isDragActive ? 'border-primary-500 bg-primary-50' : 'border-gray-300 hover:border-primary-400'
        } ${files.length > 0 ? 'bg-gray-50' : ''}`}
      >
        <input {...getInputProps()} webkitdirectory="" directory="" />
        
        {files.length === 0 ? (
          <div className="text-center">
            <svg className="mx-auto h-12 w-12 text-gray-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M9 13h6m-3-3v6m-9 1V7a2 2 0 012-2h6l2 2h6a2 2 0 012 2v8a2 2 0 01-2 2H5a2 2 0 01-2-2z" />
            </svg>
            <p className="mt-4 text-lg font-medium text-gray-900">
              {isDragActive ? 'Drop the files here' : 'Drag and drop files or directories here'}
            </p>
            <p className="mt-1 text-sm text-gray-500">
              Or click to browse
            </p>
          </div>
        ) : (
          <div>
            <div className="flex justify-between items-center mb-4">
              <h3 className="text-lg font-medium text-gray-900">
                {files.length} file{files.length !== 1 ? 's' : ''} selected
              </h3>
              <button 
                type="button"
                onClick={(e) => { e.stopPropagation(); clearAll(); }}
                className="text-sm text-primary-600 hover:text-primary-800"
              >
                Clear all
              </button>
            </div>
            <ul className="max-h-60 overflow-y-auto divide-y divide-gray-200">
              {files.slice(0, 10).map((file, index) => (
                <li key={`${file.name}-${index}`} className="py-2 flex justify-between">
                  <div className="truncate flex-1">
                    <span className="font-medium">{file.name}</span>
                    <span className="ml-2 text-sm text-gray-500">
                      {(file.size / 1024).toFixed(1)} KB
                    </span>
                  </div>
                  <button 
                    type="button"
                    onClick={(e) => { e.stopPropagation(); removeFile(index); }}
                    className="ml-2 text-sm text-red-600 hover:text-red-800"
                  >
                    Remove
                  </button>
                </li>
              ))}
              {files.length > 10 && (
                <li className="py-2 text-sm text-gray-500">
                  ... and {files.length - 10} more files
                </li>
              )}
            </ul>
            <p className="mt-2 text-sm text-gray-500">
              Drag more files to add to the selection
            </p>
          </div>
        )}
      </div>

      <div className="mt-6 flex justify-end">
        <button
          type="button"
          onClick={handleSubmit}
          disabled={files.length === 0 || isProcessing}
          className={`px-6 py-2 rounded-md text-white font-medium ${
            files.length === 0 || isProcessing
              ? 'bg-gray-300 cursor-not-allowed'
              : 'bg-primary-600 hover:bg-primary-700'
          }`}
        >
          {isProcessing ? 'Processing...' : 'Process Files'}
        </button>
      </div>
    </div>
  )
}