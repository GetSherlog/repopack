'use client'

interface ProcessingStatusProps {
  progress: number;
  message?: string;
}

export default function ProcessingStatus({ progress, message }: ProcessingStatusProps) {
  return (
    <div className="mt-8 p-6 border border-gray-200 rounded-lg bg-white">
      <h3 className="text-lg font-medium mb-4">
        {message || 'Processing Files'}
      </h3>
      
      <div className="w-full bg-gray-200 rounded-full h-4 mb-4">
        <div 
          className="bg-primary-600 h-4 rounded-full transition-all duration-300 ease-in-out"
          style={{ width: `${progress}%` }}
        ></div>
      </div>
      
      <div className="flex justify-between items-center text-sm text-gray-600">
        <span>Progress: {Math.round(progress)}%</span>
        {progress < 100 ? (
          <span className="flex items-center">
            <svg className="animate-spin -ml-1 mr-2 h-4 w-4 text-primary-600" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24">
              <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
              <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
            </svg>
            Processing
          </span>
        ) : (
          <span className="flex items-center text-green-600">
            <svg className="h-4 w-4 mr-1" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
            </svg>
            Complete
          </span>
        )}
      </div>

      <div className="mt-6 text-sm text-gray-600">
        <p>Applying filters and processing files...</p>
        <ul className="mt-2 space-y-1">
          {progress >= 20 && <li className="flex items-center">
            <svg className="h-4 w-4 mr-1 text-green-600" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
            </svg>
            Loading repository structure
          </li>}
          {progress >= 40 && <li className="flex items-center">
            <svg className="h-4 w-4 mr-1 text-green-600" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
            </svg>
            Applying .gitignore rules
          </li>}
          {progress >= 60 && <li className="flex items-center">
            <svg className="h-4 w-4 mr-1 text-green-600" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
            </svg>
            Processing files with C++ backend
          </li>}
          {progress >= 80 && <li className="flex items-center">
            <svg className="h-4 w-4 mr-1 text-green-600" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
            </svg>
            Generating output file
          </li>}
          {progress >= 100 && <li className="flex items-center">
            <svg className="h-4 w-4 mr-1 text-green-600" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
            </svg>
            Processing complete
          </li>}
        </ul>
      </div>
    </div>
  )
}