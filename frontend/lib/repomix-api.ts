// This file handles communication with the Repomix API backend
// Renamed from repomix-wasm.ts to repomix-api.ts

// Type definitions for the Repomix API responses
export interface RepomixApiResponse {
  success: boolean;
  content?: string;
  summary?: string;
  error?: string;
  contentInFile?: boolean;
  contentFilePath?: string;
  contentSnippet?: string;
  tokenCount?: number;
  tokenizer?: string;
  scoring_report?: any;
}

interface ThreadingCapabilities {
  availableThreads: number;
  serverVersion: string;
  supportsMultithreading: boolean;
}

// Extend the File interface to add our custom properties
interface RepoFile extends File {
  __repoName?: string;
}

// GitHub request body interface
interface GitHubRequestBody {
  repoUrl: string;
  format: string;
  token: string;
  include?: string;
  exclude?: string;
  count_tokens?: boolean;
  token_encoding?: string;
  tokens_only?: boolean;
  summarization_options?: any;
  verbose?: boolean;
  show_timing?: boolean;
  file_selection_strategy?: string;
  scoring_config?: any;
}

// JSZip type definitions
interface JSZipObject {
  dir: boolean;
  name: string;
  async: (type: string) => Promise<Blob>;
  // Add other properties as needed
}

interface JSZipFile {
  [path: string]: JSZipObject;
}

// Declare JSZip global from CDN
declare global {
  interface Window {
    JSZip: any; // Add JSZip to the Window interface
  }
}

// Supported tokenizer encodings
export const TOKENIZER_ENCODINGS = [
  { value: 'cl100k_base', label: 'cl100k_base - ChatGPT models' },
  { value: 'p50k_base', label: 'p50k_base - Code models' },
  { value: 'p50k_edit', label: 'p50k_edit - Edit models' },
  { value: 'r50k_base', label: 'r50k_base - GPT-3 models (Davinci)' },
  { value: 'o200k_base', label: 'o200k_base - GPT-4o models' }
];

// Helper function to format file size
function formatFileSize(bytes: number): string {
  if (bytes < 1024) return bytes + ' B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

// API configuration
const API_CONFIG = {
  baseUrl: process.env.NEXT_PUBLIC_API_URL || 'http://localhost:9000/api',
  endpoints: {
    processFiles: '/process_files',
    processRepo: '/process_repo',
    capabilities: '/capabilities',
    processUploadedDir: '/process_uploaded_dir',
    processShared: '/process_shared',
  }
};

/**
 * Checks if the API server is available
 * @returns Promise<boolean> - True if the API is available
 */
export async function isApiAvailable(): Promise<boolean> {
  try {
    const response = await fetch(`${API_CONFIG.baseUrl}${API_CONFIG.endpoints.capabilities}`, {
      method: 'GET',
      headers: {
        'Accept': 'application/json',
      },
    });
    
    return response.ok;
  } catch (error) {
    console.error('API availability check failed:', error);
    return false;
  }
}

/**
 * Process uploaded files
 * @param files Array of files to process
 * @param format Output format (plain, markdown, xml, claude_xml)
 * @param includePatterns Optional comma-separated glob patterns to include (e.g. "*.rs,*.toml")
 * @param excludePatterns Optional comma-separated glob patterns to exclude (e.g. "*.txt,*.md")
 * @param countTokens Optional flag to count tokens in the output
 * @param tokenEncoding Optional tokenizer encoding to use (default: cl100k_base)
 * @param tokensOnly Optional flag to only return token count without content
 * @param fileSelection Optional file selection strategy ('all', 'scoring')
 * @param scoringConfig Optional scoring configuration for 'scoring' strategy
 * @returns Promise with processing results
 */
export async function processFiles(
  files: File[],
  format: string,
  includePatterns: string,
  excludePatterns: string,
  countTokens: boolean,
  tokenEncoding: string,
  tokensOnly: boolean,
  fileSelection: string = 'all',
  scoringConfig?: any
): Promise<RepomixApiResponse> {
  const formData = new FormData()
  
  // Add files to form data
  files.forEach(file => {
    formData.append('files', file)
  })
  
  // Add other parameters
  formData.append('format', format)
  if (includePatterns) formData.append('include', includePatterns)
  if (excludePatterns) formData.append('exclude', excludePatterns)
  
  // Add token counting options
  if (countTokens) {
    formData.append('count_tokens', 'true')
    formData.append('token_encoding', tokenEncoding)
    if (tokensOnly) {
      formData.append('tokens_only', 'true')
    }
  }
  
  // Add file selection strategy
  formData.append('file_selection', fileSelection)
  
  // Add scoring configuration if using scoring strategy
  if (fileSelection === 'scoring' && scoringConfig) {
    Object.entries(scoringConfig).forEach(([key, value]: [string, any]) => {
      formData.append(key, value.toString())
    })
  }
  
  const response = await fetch(`${API_CONFIG.baseUrl}${API_CONFIG.endpoints.processFiles}`, {
    method: 'POST',
    body: formData
  })
  
  if (!response.ok) {
    throw new Error(`Server responded with status ${response.status}`)
  }
  
  return await response.json()
}

/**
 * Process a GitHub repository
 * @param repoUrl GitHub repository URL
 * @param format Output format (plain, markdown, xml, claude_xml)
 * @param includePatterns Optional comma-separated glob patterns to include (e.g. "*.rs,*.toml")
 * @param excludePatterns Optional comma-separated glob patterns to exclude (e.g. "*.txt,*.md")
 * @param countTokens Optional flag to count tokens in the output
 * @param tokenEncoding Optional tokenizer encoding to use (default: cl100k_base)
 * @param tokensOnly Optional flag to only return token count without content
 * @param summarizationOptions Optional smart summarization options
 * @param showVerbose Optional flag to show verbose output
 * @param showTiming Optional flag to show timing information
 * @param fileSelectionStrategy Optional file selection strategy ('all', 'scoring')
 * @param fileScoringConfig Optional scoring configuration for 'scoring' strategy
 * @returns Promise with processing results
 */
export async function processGitRepo(
  repoUrl: string, 
  format: string = 'plain',
  includePatterns: string = '',
  excludePatterns: string = '',
  countTokens: boolean = false,
  tokenEncoding: string = 'cl100k_base',
  tokensOnly: boolean = false,
  summarizationOptions: any = null,
  showVerbose: boolean = false,
  showTiming: boolean = false,
  fileSelectionStrategy: string = 'all',
  fileScoringConfig: any = null
): Promise<RepomixApiResponse> {
  try {
    const token = localStorage.getItem('githubToken') || '';
    
    // Prepare the request body
    const requestBody: GitHubRequestBody = {
      repoUrl: repoUrl,
      format: format,
      token: token
    };
    
    // Add include patterns if provided
    if (includePatterns) {
      requestBody.include = includePatterns;
    }
    
    // Add exclude patterns if provided
    if (excludePatterns) {
      requestBody.exclude = excludePatterns;
    }

    // Add token counting parameters
    if (countTokens) {
      requestBody.count_tokens = true;
      requestBody.token_encoding = tokenEncoding;
      
      if (tokensOnly) {
        requestBody.tokens_only = true;
      }
    }
    
    // Add summarization options if provided
    if (summarizationOptions) {
      requestBody.summarization_options = summarizationOptions;
    }
    
    // Add verbose and timing flags
    if (showVerbose) {
      requestBody.verbose = true;
    }
    
    if (showTiming) {
      requestBody.show_timing = true;
    }
    
    // Add file selection strategy
    if (fileSelectionStrategy !== 'all') {
      requestBody.file_selection_strategy = fileSelectionStrategy;
      
      // Add scoring configuration if using scoring strategy
      if (fileSelectionStrategy === 'scoring' && fileScoringConfig) {
        requestBody.scoring_config = fileScoringConfig;
      }
    }
    
    // Send request to API - use the existing processRepo endpoint
    const response = await fetch(`${API_CONFIG.baseUrl}${API_CONFIG.endpoints.processRepo}`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify(requestBody),
    });
    
    if (!response.ok) {
      throw new Error(`API request failed with status: ${response.status}`);
    }
    
    // Check content size before parsing
    const contentLength = response.headers.get('Content-Length');
    if (contentLength && parseInt(contentLength) > 10 * 1024 * 1024) { // > 10MB
      console.warn('Very large response detected, this may cause parsing issues');
    }
    
    // Read the response as text first (safer for large responses)
    const responseText = await response.text();
    
    // Record response size for debugging
    console.log(`Response size: ${responseText.length} bytes`);
    
    // Try to parse the JSON
    let result;
    try {
      result = JSON.parse(responseText);
    } catch (err: any) {
      console.error('Failed to parse response JSON:', err);
      return {
        success: false,
        error: `Failed to parse response: ${err.message || 'Unknown parsing error'}. Response size: ${responseText.length} bytes`,
      };
    }
    
    // Ensure we have valid content or error message
    if (result.success && (!result.content || result.content === null)) {
      console.warn('Backend returned success but no content');
      result.content = "Repository processed successfully, but no content was returned.";
    }
    
    if (!result.success && (!result.error || result.error === null)) {
      console.warn('Backend returned failure but no error message');
      result.error = "Unknown error occurred while processing repository.";
    }
    
    console.log('Received response:', result);
    if (result.contentTruncated) {
      console.warn('Content was truncated due to size limits');
    }
    
    return result as RepomixApiResponse;
  } catch (error: any) {
    if (error.name === 'AbortError') {
      console.error('Request timed out after 2 minutes');
      return {
        success: false,
        error: 'Request timed out. The repository may be too large to process.'
      };
    }
    console.error('Error processing GitHub repository:', error);
    throw error;
  }
}

/**
 * Get server capabilities
 * @returns Promise with threading capabilities
 */
export async function getServerCapabilities(): Promise<ThreadingCapabilities> {
  try {
    const response = await fetch(`${API_CONFIG.baseUrl}${API_CONFIG.endpoints.capabilities}`, {
      method: 'GET',
      headers: {
        'Accept': 'application/json',
      },
    });
    
    if (!response.ok) {
      throw new Error(`API request failed with status: ${response.status}`);
    }
    
    // Parse and return response
    const result = await response.json();
    return result as ThreadingCapabilities;
  } catch (error) {
    console.error('Failed to get server capabilities:', error);
    return {
      availableThreads: 0,
      serverVersion: 'unknown',
      supportsMultithreading: false,
    };
  }
}

// Export name-preserving functions for backward compatibility
export { processFiles as processFilesWithWasm, processGitRepo as processGitRepoWithWasm };