import React, { useState, useEffect } from 'react';

interface FileScoringOptionsProps {
  options: FileScoringConfig;
  onChange: (options: FileScoringConfig) => void;
  enabled: boolean;
}

export interface FileScoringConfig {
  // Project structure weights
  rootFilesWeight: number;
  topLevelDirsWeight: number;
  entryPointsWeight: number;
  dependencyGraphWeight: number;

  // File type weights
  sourceCodeWeight: number;
  configFilesWeight: number;
  documentationWeight: number;
  testFilesWeight: number;

  // Recency weights
  recentlyModifiedWeight: number;
  recentTimeWindowDays: number;

  // File size weights (inverse - smaller files score higher)
  fileSizeWeight: number;
  largeFileThreshold: number;

  // Code density
  codeDensityWeight: number;

  // Minimum score threshold for inclusion (0.0 - 1.0)
  inclusionThreshold: number;

  // TreeSitter usage for improved parsing
  useTreeSitter: boolean;
}

export const defaultFileScoringConfig: FileScoringConfig = {
  // Project structure weights
  rootFilesWeight: 0.9,
  topLevelDirsWeight: 0.8,
  entryPointsWeight: 0.8,
  dependencyGraphWeight: 0.7,

  // File type weights
  sourceCodeWeight: 0.8,
  configFilesWeight: 0.7,
  documentationWeight: 0.6,
  testFilesWeight: 0.5,

  // Recency weights
  recentlyModifiedWeight: 0.7,
  recentTimeWindowDays: 7,

  // File size weights
  fileSizeWeight: 0.4,
  largeFileThreshold: 1000000,

  // Code density
  codeDensityWeight: 0.5,

  // Threshold
  inclusionThreshold: 0.3,

  // TreeSitter usage
  useTreeSitter: true,
};

const FileScoringOptions: React.FC<FileScoringOptionsProps> = ({ options, onChange, enabled }) => {
  const handleChange = (field: keyof FileScoringConfig, value: any) => {
    onChange({
      ...options,
      [field]: value,
    });
  };

  // Format the slider value to show as percentage
  const formatPercent = (value: number) => `${Math.round(value * 100)}%`;
  
  // Format the threshold slider value
  const formatThreshold = (value: number) => value.toFixed(2);

  if (!enabled) {
    return null;
  }

  return (
    <div className="space-y-4 p-4 bg-gray-50 rounded-md border border-gray-200">
      <h3 className="text-lg font-medium text-gray-900">File Scoring Configuration</h3>
      
      <div className="space-y-4">
        <div className="border-b pb-4">
          <h4 className="text-md font-medium text-gray-800 mb-2">Project Structure Weights</h4>
          <div className="space-y-3">
            <div>
              <label className="block text-sm text-gray-700">Root Files Weight: {formatPercent(options.rootFilesWeight)}</label>
              <input
                type="range"
                min="0"
                max="1"
                step="0.05"
                value={options.rootFilesWeight}
                onChange={(e) => handleChange('rootFilesWeight', parseFloat(e.target.value))}
                className="w-full"
              />
              <p className="text-xs text-gray-500">Importance of root-level files (README, package.json, etc.)</p>
            </div>
            
            <div>
              <label className="block text-sm text-gray-700">Top-Level Directories Weight: {formatPercent(options.topLevelDirsWeight)}</label>
              <input
                type="range"
                min="0"
                max="1"
                step="0.05"
                value={options.topLevelDirsWeight}
                onChange={(e) => handleChange('topLevelDirsWeight', parseFloat(e.target.value))}
                className="w-full"
              />
              <p className="text-xs text-gray-500">Importance of files in key directories (src, lib, etc.)</p>
            </div>
            
            <div>
              <label className="block text-sm text-gray-700">Entry Points Weight: {formatPercent(options.entryPointsWeight)}</label>
              <input
                type="range"
                min="0"
                max="1"
                step="0.05"
                value={options.entryPointsWeight}
                onChange={(e) => handleChange('entryPointsWeight', parseFloat(e.target.value))}
                className="w-full"
              />
              <p className="text-xs text-gray-500">Importance of entry point files (index.js, main.py, etc.)</p>
            </div>
            
            <div>
              <label className="block text-sm text-gray-700">Dependency Graph Weight: {formatPercent(options.dependencyGraphWeight)}</label>
              <input
                type="range"
                min="0"
                max="1"
                step="0.05"
                value={options.dependencyGraphWeight}
                onChange={(e) => handleChange('dependencyGraphWeight', parseFloat(e.target.value))}
                className="w-full"
              />
              <p className="text-xs text-gray-500">Importance of files that are widely imported/referenced</p>
            </div>
          </div>
        </div>
        
        <div className="border-b pb-4">
          <h4 className="text-md font-medium text-gray-800 mb-2">File Type Weights</h4>
          <div className="space-y-3">
            <div>
              <label className="block text-sm text-gray-700">Source Code Weight: {formatPercent(options.sourceCodeWeight)}</label>
              <input
                type="range"
                min="0"
                max="1"
                step="0.05"
                value={options.sourceCodeWeight}
                onChange={(e) => handleChange('sourceCodeWeight', parseFloat(e.target.value))}
                className="w-full"
              />
            </div>
            
            <div>
              <label className="block text-sm text-gray-700">Config Files Weight: {formatPercent(options.configFilesWeight)}</label>
              <input
                type="range"
                min="0"
                max="1"
                step="0.05"
                value={options.configFilesWeight}
                onChange={(e) => handleChange('configFilesWeight', parseFloat(e.target.value))}
                className="w-full"
              />
            </div>
            
            <div>
              <label className="block text-sm text-gray-700">Documentation Weight: {formatPercent(options.documentationWeight)}</label>
              <input
                type="range"
                min="0"
                max="1"
                step="0.05"
                value={options.documentationWeight}
                onChange={(e) => handleChange('documentationWeight', parseFloat(e.target.value))}
                className="w-full"
              />
            </div>
            
            <div>
              <label className="block text-sm text-gray-700">Test Files Weight: {formatPercent(options.testFilesWeight)}</label>
              <input
                type="range"
                min="0"
                max="1"
                step="0.05"
                value={options.testFilesWeight}
                onChange={(e) => handleChange('testFilesWeight', parseFloat(e.target.value))}
                className="w-full"
              />
            </div>
          </div>
        </div>
        
        <div className="border-b pb-4">
          <h4 className="text-md font-medium text-gray-800 mb-2">Other Factors</h4>
          <div className="space-y-3">
            <div>
              <label className="block text-sm text-gray-700">Recently Modified Weight: {formatPercent(options.recentlyModifiedWeight)}</label>
              <input
                type="range"
                min="0"
                max="1"
                step="0.05"
                value={options.recentlyModifiedWeight}
                onChange={(e) => handleChange('recentlyModifiedWeight', parseFloat(e.target.value))}
                className="w-full"
              />
              <p className="text-xs text-gray-500">Importance of recently modified files</p>
            </div>
            
            <div>
              <label className="block text-sm text-gray-700">Recent Time Window (days)</label>
              <input
                type="number"
                min="1"
                max="90"
                value={options.recentTimeWindowDays}
                onChange={(e) => handleChange('recentTimeWindowDays', parseInt(e.target.value))}
                className="w-full p-2 border rounded"
              />
              <p className="text-xs text-gray-500">What constitutes "recent" in days</p>
            </div>
            
            <div>
              <label className="block text-sm text-gray-700">File Size Weight: {formatPercent(options.fileSizeWeight)}</label>
              <input
                type="range"
                min="0"
                max="1"
                step="0.05"
                value={options.fileSizeWeight}
                onChange={(e) => handleChange('fileSizeWeight', parseFloat(e.target.value))}
                className="w-full"
              />
              <p className="text-xs text-gray-500">Smaller files get higher scores (inverse relationship)</p>
            </div>
            
            <div>
              <label className="block text-sm text-gray-700">Large File Threshold (bytes)</label>
              <input
                type="number"
                min="1000"
                max="10000000"
                step="1000"
                value={options.largeFileThreshold}
                onChange={(e) => handleChange('largeFileThreshold', parseInt(e.target.value))}
                className="w-full p-2 border rounded"
              />
              <p className="text-xs text-gray-500">Size threshold for what's considered a large file</p>
            </div>
            
            <div>
              <label className="block text-sm text-gray-700">Code Density Weight: {formatPercent(options.codeDensityWeight)}</label>
              <input
                type="range"
                min="0"
                max="1"
                step="0.05"
                value={options.codeDensityWeight}
                onChange={(e) => handleChange('codeDensityWeight', parseFloat(e.target.value))}
                className="w-full"
              />
              <p className="text-xs text-gray-500">Importance of code-to-comment ratio and structure</p>
            </div>
          </div>
        </div>
        
        <div>
          <h4 className="text-md font-medium text-gray-800 mb-2">Inclusion Settings</h4>
          <div className="space-y-3">
            <div>
              <label className="block text-sm text-gray-700">Inclusion Threshold: {formatThreshold(options.inclusionThreshold)}</label>
              <input
                type="range"
                min="0"
                max="1"
                step="0.05"
                value={options.inclusionThreshold}
                onChange={(e) => handleChange('inclusionThreshold', parseFloat(e.target.value))}
                className="w-full"
              />
              <p className="text-xs text-gray-500">Minimum score needed for file inclusion (0-1)</p>
            </div>
            
            <div className="flex items-center">
              <input
                type="checkbox"
                id="useTreeSitter"
                checked={options.useTreeSitter}
                onChange={(e) => handleChange('useTreeSitter', e.target.checked)}
                className="h-4 w-4 text-blue-600 border-gray-300 rounded"
              />
              <label htmlFor="useTreeSitter" className="ml-2 block text-sm text-gray-700">
                Use TreeSitter for improved code analysis
              </label>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default FileScoringOptions; 