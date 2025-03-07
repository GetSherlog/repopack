import Link from 'next/link'
import Image from 'next/image'

export default function Home() {
  return (
    <main className="min-h-screen p-8 flex flex-col items-center justify-center bg-background">
      <div className="max-w-5xl w-full flex flex-col items-center">
        <h1 className="text-5xl font-bold text-center mb-4">
          Sherlog-repopack
        </h1>
        <p className="text-xl text-muted-foreground text-center mb-12">
          Pack your repository into a single text file for AI consumption
        </p>

        <div className="grid grid-cols-1 md:grid-cols-2 gap-8 w-full max-w-3xl">
          <Link href="/upload" 
                className="p-8 border border-border rounded-lg hover:bg-secondary transition-colors flex flex-col items-center">
            <div className="rounded-full bg-secondary p-4 mb-4">
              <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className="text-primary">
                <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path>
                <polyline points="17 8 12 3 7 8"></polyline>
                <line x1="12" y1="3" x2="12" y2="15"></line>
              </svg>
            </div>
            <h2 className="text-2xl font-semibold mb-2">Upload Files</h2>
            <p className="text-center text-muted-foreground">
              Upload local files and directories to process
            </p>
          </Link>

          <Link href="/github" 
                className="p-8 border border-border rounded-lg hover:bg-secondary transition-colors flex flex-col items-center">
            <div className="rounded-full bg-secondary p-4 mb-4">
              <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className="text-primary">
                <path d="M9 19c-5 1.5-5-2.5-7-3m14 6v-3.87a3.37 3.37 0 0 0-.94-2.61c3.14-.35 6.44-1.54 6.44-7A5.44 5.44 0 0 0 20 4.77 5.07 5.07 0 0 0 19.91 1S18.73.65 16 2.48a13.38 13.38 0 0 0-7 0C6.27.65 5.09 1 5.09 1A5.07 5.07 0 0 0 5 4.77a5.44 5.44 0 0 0-1.5 3.78c0 5.42 3.3 6.61 6.44 7A3.37 3.37 0 0 0 9 18.13V22"></path>
              </svg>
            </div>
            <h2 className="text-2xl font-semibold mb-2">GitHub Repository</h2>
            <p className="text-center text-muted-foreground">
              Enter a GitHub repository URL to process
            </p>
          </Link>
        </div>

        <div className="mt-16 bg-card p-8 rounded-lg max-w-3xl w-full shadow-sm border border-border">
          <h2 className="text-2xl font-semibold mb-4">Why use Sherlog-repopack?</h2>
          <ul className="space-y-4">
            <li className="flex items-start">
              <svg className="h-6 w-6 text-primary mr-2 flex-shrink-0" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
              </svg>
              <span>High-performance C++ processing with WASM</span>
            </li>
            <li className="flex items-start">
              <svg className="h-6 w-6 text-primary mr-2 flex-shrink-0" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
              </svg>
              <span>Smart filtering of binary and build files</span>
            </li>
            <li className="flex items-start">
              <svg className="h-6 w-6 text-primary mr-2 flex-shrink-0" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
              </svg>
              <span>Respect for .gitignore rules and file sizes</span>
            </li>
            <li className="flex items-start">
              <svg className="h-6 w-6 text-primary mr-2 flex-shrink-0" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
              </svg>
              <span>Optimized for AI context windows</span>
            </li>
          </ul>
        </div>
      </div>
    </main>
  )
}