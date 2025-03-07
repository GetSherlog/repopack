import React, { useState } from 'react';

interface FileScoringOptionsProps {
  config: FileScoringConfig;
  onChange: (config: FileScoringConfig) => void;
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

const FileScoringOptions: React.FC<FileScoringOptionsProps> = ({ config, onChange }) => {
  const [showAdvanced, setShowAdvanced] = useState(false);

  const handleChange = (field: keyof FileScoringConfig, value: any) => {
    onChange({ ...config, [field]: value });
  };

  const SliderInput = ({ field, label, min = 0, max = 1, step = 0.1 }: 
    { field: keyof FileScoringConfig, label: string, min?: number, max?: number, step?: number }) => (
    <div className="mb-2">
      <div className="flex justify-between items-center">
        <label className="text-sm text-gray-700">
          {label} ({config[field]})
        </label>
      </div>
      <input
        type="range"
        min={min}
        max={max}
        step={step}
        value={config[field] as number}
        onChange={(e) => handleChange(field, parseFloat(e.target.value))}
        className="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer"
      />
    </div>
  );

  return (
    <div>
      <h3 className="text-lg font-semibold mb-2">File Scoring Options</h3>
      <p className="text-sm text-gray-500 mb-4">
        Configure how files are scored and selected for inclusion
      </p>

      <div className="mb-4">
        <label className="block text-sm font-medium text-gray-700 mb-1">
          Inclusion Threshold
        </label>
        <div className="flex items-center">
          <input
            type="range"
            min="0"
            max="1"
            step="0.05"
            value={config.inclusionThreshold}
            onChange={(e) => handleChange('inclusionThreshold', parseFloat(e.target.value))}
            className="w-full h-2 bg-gray-200 rounded-lg appearance-none cursor-pointer mr-2"
          />
          <span className="text-sm text-gray-700">{config.inclusionThreshold.toFixed(2)}</span>
        </div>
        <p className="mt-1 text-xs text-gray-500">
          Files with scores below this threshold will be excluded
        </p>
      </div>

      <div className="flex items-center mb-4">
        <input
          type="checkbox"
          id="useTreeSitter"
          checked={config.useTreeSitter}
          onChange={(e) => handleChange('useTreeSitter', e.target.checked)}
          className="h-4 w-4 text-blue-600 focus:ring-blue-500"
        />
        <label htmlFor="useTreeSitter" className="ml-2 text-sm text-gray-700">
          Use Tree-sitter for improved code analysis
        </label>
      </div>

      <div className="mb-2">
        <button
          type="button"
          onClick={() => setShowAdvanced(!showAdvanced)}
          className="text-sm font-medium text-blue-600 hover:text-blue-800 focus:outline-none"
        >
          {showAdvanced ? '- Hide Advanced Scoring Options' : '+ Show Advanced Scoring Options'}
        </button>
      </div>

      {showAdvanced && (
        <div className="p-3 border border-gray-200 rounded-md">
          <h4 className="font-medium text-sm text-gray-800 mb-2">Project Structure Weights</h4>
          <SliderInput field="rootFilesWeight" label="Root Files" />
          <SliderInput field="topLevelDirsWeight" label="Top-level Directories" />
          <SliderInput field="entryPointsWeight" label="Entry Points" />
          <SliderInput field="dependencyGraphWeight" label="Dependency Graph" />

          <h4 className="font-medium text-sm text-gray-800 mt-4 mb-2">File Type Weights</h4>
          <SliderInput field="sourceCodeWeight" label="Source Code" />
          <SliderInput field="configFilesWeight" label="Configuration Files" />
          <SliderInput field="documentationWeight" label="Documentation" />
          <SliderInput field="testFilesWeight" label="Test Files" />

          <h4 className="font-medium text-sm text-gray-800 mt-4 mb-2">Recency Weights</h4>
          <SliderInput field="recentlyModifiedWeight" label="Recently Modified Files" />
          <div className="mb-2">
            <label className="text-sm text-gray-700">
              Recent Time Window (days)
            </label>
            <input
              type="number"
              value={config.recentTimeWindowDays}
              onChange={(e) => handleChange('recentTimeWindowDays', parseInt(e.target.value))}
              className="w-full px-3 py-1 text-sm border border-gray-300 rounded"
              min="1"
              max="365"
            />
          </div>

          <h4 className="font-medium text-sm text-gray-800 mt-4 mb-2">File Size Weights</h4>
          <SliderInput field="fileSizeWeight" label="File Size (inverse)" />
          <div className="mb-2">
            <label className="text-sm text-gray-700">
              Large File Threshold (bytes)
            </label>
            <input
              type="number"
              value={config.largeFileThreshold}
              onChange={(e) => handleChange('largeFileThreshold', parseInt(e.target.value))}
              className="w-full px-3 py-1 text-sm border border-gray-300 rounded"
              min="1000"
              max="10000000"
              step="1000"
            />
          </div>

          <h4 className="font-medium text-sm text-gray-800 mt-4 mb-2">Code Density</h4>
          <SliderInput field="codeDensityWeight" label="Code Density" />
        </div>
      )}
    </div>
  );
};

export default FileScoringOptions; 