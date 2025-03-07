// This file handles communication with the Repomix API backend
// Renamed from repomix-wasm.ts to repomix-api.ts

// Type definitions for the Repomix API responses
interface RepomixApiResponse {
  success: boolean;
  content?: string;
  summary?: string;
  error?: string;
  contentInFile?: boolean;
  contentFilePath?: string;
  contentSnippet?: string;
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
 * Process files using the Repomix API
 * @param files Array of files to process
 * @param format Output format (plain, markdown, xml)
 * @returns Promise with processing results
 */
export async function processFiles(files: File[], format: string = 'plain'): Promise<RepomixApiResponse> {
  try {
    // Validate input
    if (!files || files.length === 0) {
      throw new Error('No files provided');
    }
    
    // Create form data with files
    const formData = new FormData();
    for (let i = 0; i < files.length; i++) {
      formData.append('file', files[i], files[i].name);
    }
    
    // Add format parameter
    formData.append('format', format);
    
    // Send request to API
    const response = await fetch(`${API_CONFIG.baseUrl}${API_CONFIG.endpoints.processFiles}`, {
      method: 'POST',
      body: formData,
    });
    
    if (!response.ok) {
      throw new Error(`API request failed with status: ${response.status}`);
    }
    
    // Parse and return response
    const result = await response.json();
    return result as RepomixApiResponse;
    
  } catch (error) {
    console.error('Error processing files:', error);
    throw error;
  }
}

/**
 * Process a GitHub repository using the Repomix API
 * @param repoUrl URL of the GitHub repository to process
 * @param format Output format (plain, markdown, xml)
 * @returns Promise with processing results
 */
export async function processGitRepo(repoUrl: string, format: string = 'plain'): Promise<RepomixApiResponse> {
  try {
    // Validate input
    if (!repoUrl) {
      throw new Error('No repository URL provided');
    }

    console.log(`Processing GitHub repository: ${repoUrl}`);
    
    // Get GitHub token from localStorage if available
    const token = localStorage.getItem('githubToken') || '';
    
    // Create simple request with just the URL and format
    // The backend will handle fetching the repository
    const requestBody = {
      repoUrl,
      format,
      token
    };
    
    console.log('Sending GitHub repository request to backend...');
    
    // Use a higher timeout for large responses
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 120000); // 2 minute timeout
    
    try {
      const response = await fetch(`${API_CONFIG.baseUrl}${API_CONFIG.endpoints.processShared}`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(requestBody),
        signal: controller.signal
      });
      
      clearTimeout(timeoutId);
      
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
    } finally {
      clearTimeout(timeoutId);
    }
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