import React, { useState, useEffect } from 'react';
import { Box, LinearProgress, Typography, Paper, Grid, Chip } from '@mui/material';

// Progress information interface
interface ProgressInfo {
  id: string;
  totalFiles: number;
  processedFiles: number;
  skippedFiles: number;
  errorFiles: number;
  currentFile: string;
  isComplete: boolean;
  percentage: number;
  elapsedMs: number;
}

interface ProcessingProgressProps {
  jobId: string;
  onComplete?: (success: boolean) => void;
  apiUrl?: string;
}

const ProcessingProgress: React.FC<ProcessingProgressProps> = ({ 
  jobId, 
  onComplete,
  apiUrl = process.env.NEXT_PUBLIC_API_URL || 'http://localhost:9000/api'
}) => {
  const [progress, setProgress] = useState<ProgressInfo | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [polling, setPolling] = useState<boolean>(true);

  // Format elapsed time in a human-readable format
  const formatElapsedTime = (ms: number): string => {
    if (ms < 1000) return `${ms}ms`;
    
    const seconds = Math.floor(ms / 1000);
    if (seconds < 60) return `${seconds}s`;
    
    const minutes = Math.floor(seconds / 60);
    const remainingSeconds = seconds % 60;
    return `${minutes}m ${remainingSeconds}s`;
  };

  useEffect(() => {
    if (!jobId || !polling) return;

    // Function to fetch progress
    const fetchProgress = async () => {
      try {
        const response = await fetch(`${apiUrl}/progress/${jobId}`);
        
        if (!response.ok) {
          if (response.status === 404) {
            setError(`Job ${jobId} not found`);
            setPolling(false);
            return;
          }
          throw new Error(`Server responded with status ${response.status}`);
        }
        
        const data = await response.json();
        setProgress(data);
        
        // If processing is complete, stop polling and call onComplete callback
        if (data.isComplete) {
          setPolling(false);
          if (onComplete) {
            onComplete(true);
          }
        }
      } catch (err) {
        console.error('Error fetching progress:', err);
        setError(`Failed to fetch progress: ${err.message}`);
        setPolling(false);
      }
    };

    // Fetch progress immediately
    fetchProgress();
    
    // Set up polling interval (every 1 second)
    const intervalId = setInterval(fetchProgress, 1000);
    
    // Clean up interval on unmount or when polling stops
    return () => clearInterval(intervalId);
  }, [jobId, polling, apiUrl, onComplete]);

  if (error) {
    return (
      <Paper elevation={2} sx={{ p: 3, mb: 3, bgcolor: '#fff8f8' }}>
        <Typography color="error" variant="h6">Error</Typography>
        <Typography>{error}</Typography>
      </Paper>
    );
  }

  if (!progress) {
    return (
      <Paper elevation={2} sx={{ p: 3, mb: 3 }}>
        <Typography variant="h6" gutterBottom>Processing Files</Typography>
        <Typography variant="body2" color="textSecondary" gutterBottom>
          Initializing...
        </Typography>
        <LinearProgress />
      </Paper>
    );
  }

  return (
    <Paper elevation={2} sx={{ p: 3, mb: 3 }}>
      <Typography variant="h6" gutterBottom>Processing Files</Typography>
      
      <Grid container spacing={2} sx={{ mb: 2 }}>
        <Grid item xs={12} sm={6}>
          <Typography variant="body2" color="textSecondary">
            Progress: {progress.percentage.toFixed(1)}%
          </Typography>
          <LinearProgress 
            variant="determinate" 
            value={progress.percentage} 
            sx={{ height: 10, borderRadius: 5, my: 1 }}
          />
        </Grid>
        <Grid item xs={12} sm={6}>
          <Box sx={{ display: 'flex', justifyContent: 'space-between', flexWrap: 'wrap', gap: 1 }}>
            <Chip 
              label={`Files: ${progress.processedFiles}/${progress.totalFiles}`} 
              color="primary" 
              variant="outlined" 
              size="small"
            />
            <Chip 
              label={`Skipped: ${progress.skippedFiles}`} 
              color="secondary" 
              variant="outlined" 
              size="small"
            />
            <Chip 
              label={`Errors: ${progress.errorFiles}`} 
              color="error" 
              variant="outlined" 
              size="small"
            />
            <Chip 
              label={`Time: ${formatElapsedTime(progress.elapsedMs)}`} 
              color="default" 
              variant="outlined" 
              size="small"
            />
          </Box>
        </Grid>
      </Grid>
      
      {progress.currentFile && (
        <Box sx={{ mt: 2 }}>
          <Typography variant="body2" color="textSecondary">
            Current file:
          </Typography>
          <Typography variant="body2" sx={{ 
            fontFamily: 'monospace', 
            bgcolor: '#f5f5f5', 
            p: 1, 
            borderRadius: 1,
            overflow: 'hidden',
            textOverflow: 'ellipsis',
            whiteSpace: 'nowrap'
          }}>
            {progress.currentFile}
          </Typography>
        </Box>
      )}
      
      {progress.isComplete && (
        <Box sx={{ mt: 2, p: 1, bgcolor: '#f0f7f0', borderRadius: 1 }}>
          <Typography color="success.main">
            Processing complete! ({formatElapsedTime(progress.elapsedMs)})
          </Typography>
        </Box>
      )}
    </Paper>
  );
};

export default ProcessingProgress; 