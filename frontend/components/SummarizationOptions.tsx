import React, { useState, useEffect } from 'react';

interface SummarizationOptionsProps {
  options: SummarizationOptions;
  onChange: (options: SummarizationOptions) => void;
}

export interface SummarizationOptions {
  enabled: boolean;
  includeFirstNLines: boolean;
  firstNLinesCount: number;
  includeSignatures: boolean;
  includeDocstrings: boolean;
  includeSnippets: boolean;
  snippetsCount: number;
  includeReadme: boolean;
  useTreeSitter: boolean;
  fileSizeThreshold: number;
  maxSummaryLines: number;
  
  // Named Entity Recognition options
  includeEntityRecognition: boolean;
  
  // NER method selection
  nerMethod: 'Regex' | 'TreeSitter' | 'ML' | 'Hybrid';
  
  // ML-NER specific options
  useMLForLargeFiles: boolean;
  mlNerSizeThreshold: number;
  mlModelPath: string;
  cacheMLResults: boolean;
  mlConfidenceThreshold: number;
  maxMLProcessingTimeMs: number;
  
  // Entity types
  includeClassNames: boolean;
  includeFunctionNames: boolean;
  includeVariableNames: boolean;
  includeEnumValues: boolean;
  includeImports: boolean;
  maxEntities: number;
  groupEntitiesByType: boolean;
  
  // Advanced visualization options
  includeEntityRelationships: boolean;
  generateEntityGraph: boolean;
}

export const defaultSummarizationOptions: SummarizationOptions = {
  enabled: false,
  includeFirstNLines: true,
  firstNLinesCount: 50,
  includeSignatures: true,
  includeDocstrings: true,
  includeSnippets: false,
  snippetsCount: 3,
  includeReadme: true,
  useTreeSitter: true,
  fileSizeThreshold: 10240,
  maxSummaryLines: 200,
  
  // Named Entity Recognition options
  includeEntityRecognition: false,
  
  // NER method selection
  nerMethod: 'Regex',
  
  // ML-NER specific options
  useMLForLargeFiles: false,
  mlNerSizeThreshold: 102400,
  mlModelPath: '',
  cacheMLResults: true,
  mlConfidenceThreshold: 0.7,
  maxMLProcessingTimeMs: 5000,
  
  // Entity types
  includeClassNames: true,
  includeFunctionNames: true,
  includeVariableNames: true,
  includeEnumValues: true,
  includeImports: true,
  maxEntities: 100,
  groupEntitiesByType: true,
  
  // Advanced visualization options
  includeEntityRelationships: false,
  generateEntityGraph: false
};

const SummarizationOptions: React.FC<SummarizationOptionsProps> = ({ options, onChange }) => {
  const handleChange = (field: keyof SummarizationOptions, value: any) => {
    onChange({ ...options, [field]: value });
  };

  return (
    <div className="bg-gray-50 p-4 mb-4 rounded shadow">
      <h3 className="text-lg font-semibold mb-2">Smart Summarization Options</h3>
      
      <div className="grid grid-cols-1 gap-2">
        <div className="flex items-center">
          <input
            id="summarization-enabled"
            type="checkbox"
            checked={options.enabled}
            onChange={(e) => handleChange('enabled', e.target.checked)}
            className="form-checkbox h-4 w-4 text-blue-600"
          />
          <label htmlFor="summarization-enabled" className="ml-2 text-sm text-gray-700">
            Enable Smart File Summarization
          </label>
        </div>
        
        {options.enabled && (
          <>
            <div className="ml-4 mt-2 space-y-2">
              <h4 className="font-medium text-sm text-gray-800">Basic Summarization</h4>
              
              <div className="flex items-center">
                <input
                  id="include-first-n-lines"
                  type="checkbox"
                  checked={options.includeFirstNLines}
                  onChange={(e) => handleChange('includeFirstNLines', e.target.checked)}
                  className="form-checkbox h-4 w-4 text-blue-600"
                />
                <label htmlFor="include-first-n-lines" className="ml-2 text-sm text-gray-700">
                  Include first N lines
                </label>
                {options.includeFirstNLines && (
                  <input
                    type="number"
                    value={options.firstNLinesCount}
                    onChange={(e) => handleChange('firstNLinesCount', parseInt(e.target.value, 10))}
                    className="ml-2 w-16 text-sm border rounded px-2 py-1"
                    min="1"
                    max="1000"
                  />
                )}
              </div>
              
              <div className="flex items-center">
                <input
                  id="include-signatures"
                  type="checkbox"
                  checked={options.includeSignatures}
                  onChange={(e) => handleChange('includeSignatures', e.target.checked)}
                  className="form-checkbox h-4 w-4 text-blue-600"
                />
                <label htmlFor="include-signatures" className="ml-2 text-sm text-gray-700">
                  Include function/class signatures
                </label>
              </div>
              
              <div className="flex items-center">
                <input
                  id="include-docstrings"
                  type="checkbox"
                  checked={options.includeDocstrings}
                  onChange={(e) => handleChange('includeDocstrings', e.target.checked)}
                  className="form-checkbox h-4 w-4 text-blue-600"
                />
                <label htmlFor="include-docstrings" className="ml-2 text-sm text-gray-700">
                  Include docstrings and comments
                </label>
              </div>
              
              <div className="flex items-center">
                <input
                  id="include-snippets"
                  type="checkbox"
                  checked={options.includeSnippets}
                  onChange={(e) => handleChange('includeSnippets', e.target.checked)}
                  className="form-checkbox h-4 w-4 text-blue-600"
                />
                <label htmlFor="include-snippets" className="ml-2 text-sm text-gray-700">
                  Include representative snippets
                </label>
                {options.includeSnippets && (
                  <input
                    type="number"
                    value={options.snippetsCount}
                    onChange={(e) => handleChange('snippetsCount', parseInt(e.target.value, 10))}
                    className="ml-2 w-16 text-sm border rounded px-2 py-1"
                    min="1"
                    max="10"
                  />
                )}
              </div>
              
              <div className="flex items-center">
                <input
                  id="include-readme"
                  type="checkbox"
                  checked={options.includeReadme}
                  onChange={(e) => handleChange('includeReadme', e.target.checked)}
                  className="form-checkbox h-4 w-4 text-blue-600"
                />
                <label htmlFor="include-readme" className="ml-2 text-sm text-gray-700">
                  Include full README files
                </label>
              </div>
            </div>
            
            {/* Named Entity Recognition Options */}
            <div className="ml-4 mt-4 space-y-2 border-t pt-2">
              <h4 className="font-medium text-sm text-gray-800">Named Entity Recognition</h4>
              
              <div className="flex items-center">
                <input
                  id="include-entity-recognition"
                  type="checkbox"
                  checked={options.includeEntityRecognition}
                  onChange={(e) => handleChange('includeEntityRecognition', e.target.checked)}
                  className="form-checkbox h-4 w-4 text-blue-600"
                />
                <label htmlFor="include-entity-recognition" className="ml-2 text-sm text-gray-700">
                  Enable Named Entity Recognition
                </label>
              </div>
              
              {options.includeEntityRecognition && (
                <div className="ml-4 space-y-2">
                  <div className="flex items-center">
                    <label htmlFor="ner-method" className="text-sm text-gray-700 mr-2">
                      NER Method:
                    </label>
                    <select
                      id="ner-method"
                      value={options.nerMethod}
                      onChange={(e) => handleChange('nerMethod', e.target.value)}
                      className="ml-2 text-sm border rounded px-2 py-1"
                    >
                      <option value="Regex">Regex (fastest, basic)</option>
                      <option value="TreeSitter">Tree-sitter (balanced)</option>
                      <option value="ML">Machine Learning (accurate, slow)</option>
                      <option value="Hybrid">Hybrid (smart selection)</option>
                    </select>
                  </div>
                  
                  {(options.nerMethod === 'ML' || options.nerMethod === 'Hybrid') && (
                    <div className="ml-4 border-l-2 pl-2 border-gray-200 space-y-2">
                      <h5 className="font-medium text-sm text-gray-700">ML Options</h5>
                      
                      {options.nerMethod === 'Hybrid' && (
                        <div className="flex items-center">
                          <input
                            id="use-ml-for-large"
                            type="checkbox"
                            checked={options.useMLForLargeFiles}
                            onChange={(e) => handleChange('useMLForLargeFiles', e.target.checked)}
                            className="form-checkbox h-4 w-4 text-blue-600"
                          />
                          <label htmlFor="use-ml-for-large" className="ml-2 text-sm text-gray-700">
                            Use ML for large files only
                          </label>
                        </div>
                      )}
                      
                      <div className="flex items-center">
                        <label htmlFor="ml-size-threshold" className="text-sm text-gray-700">
                          Size threshold for ML (bytes):
                        </label>
                        <input
                          id="ml-size-threshold"
                          type="number"
                          value={options.mlNerSizeThreshold}
                          onChange={(e) => handleChange('mlNerSizeThreshold', parseInt(e.target.value, 10))}
                          className="ml-2 w-24 text-sm border rounded px-2 py-1"
                          min="1024"
                          step="1024"
                        />
                      </div>
                      
                      <div className="flex items-center">
                        <input
                          id="cache-ml-results"
                          type="checkbox"
                          checked={options.cacheMLResults}
                          onChange={(e) => handleChange('cacheMLResults', e.target.checked)}
                          className="form-checkbox h-4 w-4 text-blue-600"
                        />
                        <label htmlFor="cache-ml-results" className="ml-2 text-sm text-gray-700">
                          Cache ML results
                        </label>
                      </div>
                      
                      <div className="flex items-center">
                        <label htmlFor="ml-confidence" className="text-sm text-gray-700">
                          ML confidence threshold:
                        </label>
                        <input
                          id="ml-confidence"
                          type="number"
                          value={options.mlConfidenceThreshold}
                          onChange={(e) => handleChange('mlConfidenceThreshold', parseFloat(e.target.value))}
                          className="ml-2 w-16 text-sm border rounded px-2 py-1"
                          min="0"
                          max="1"
                          step="0.05"
                        />
                      </div>
                      
                      <div className="flex items-center">
                        <label htmlFor="max-ml-time" className="text-sm text-gray-700">
                          Max ML processing time (ms):
                        </label>
                        <input
                          id="max-ml-time"
                          type="number"
                          value={options.maxMLProcessingTimeMs}
                          onChange={(e) => handleChange('maxMLProcessingTimeMs', parseInt(e.target.value, 10))}
                          className="ml-2 w-24 text-sm border rounded px-2 py-1"
                          min="100"
                          step="100"
                        />
                      </div>
                      
                      <div className="flex items-center">
                        <label htmlFor="ml-model-path" className="text-sm text-gray-700">
                          Custom ML model path (optional):
                        </label>
                        <input
                          id="ml-model-path"
                          type="text"
                          value={options.mlModelPath}
                          onChange={(e) => handleChange('mlModelPath', e.target.value)}
                          className="ml-2 w-48 text-sm border rounded px-2 py-1"
                          placeholder="Leave empty for default"
                        />
                      </div>
                    </div>
                  )}
                  
                  <div className="flex items-center">
                    <input
                      id="include-class-names"
                      type="checkbox"
                      checked={options.includeClassNames}
                      onChange={(e) => handleChange('includeClassNames', e.target.checked)}
                      className="form-checkbox h-4 w-4 text-blue-600"
                    />
                    <label htmlFor="include-class-names" className="ml-2 text-sm text-gray-700">
                      Include class names
                    </label>
                  </div>
                  
                  <div className="flex items-center">
                    <input
                      id="include-function-names"
                      type="checkbox"
                      checked={options.includeFunctionNames}
                      onChange={(e) => handleChange('includeFunctionNames', e.target.checked)}
                      className="form-checkbox h-4 w-4 text-blue-600"
                    />
                    <label htmlFor="include-function-names" className="ml-2 text-sm text-gray-700">
                      Include function/method names
                    </label>
                  </div>
                  
                  <div className="flex items-center">
                    <input
                      id="include-variable-names"
                      type="checkbox"
                      checked={options.includeVariableNames}
                      onChange={(e) => handleChange('includeVariableNames', e.target.checked)}
                      className="form-checkbox h-4 w-4 text-blue-600"
                    />
                    <label htmlFor="include-variable-names" className="ml-2 text-sm text-gray-700">
                      Include variable names
                    </label>
                  </div>
                  
                  <div className="flex items-center">
                    <input
                      id="include-enum-values"
                      type="checkbox"
                      checked={options.includeEnumValues}
                      onChange={(e) => handleChange('includeEnumValues', e.target.checked)}
                      className="form-checkbox h-4 w-4 text-blue-600"
                    />
                    <label htmlFor="include-enum-values" className="ml-2 text-sm text-gray-700">
                      Include enum values
                    </label>
                  </div>
                  
                  <div className="flex items-center">
                    <input
                      id="include-imports"
                      type="checkbox"
                      checked={options.includeImports}
                      onChange={(e) => handleChange('includeImports', e.target.checked)}
                      className="form-checkbox h-4 w-4 text-blue-600"
                    />
                    <label htmlFor="include-imports" className="ml-2 text-sm text-gray-700">
                      Include imports/includes
                    </label>
                  </div>
                  
                  <div className="flex items-center">
                    <input
                      id="group-entities"
                      type="checkbox"
                      checked={options.groupEntitiesByType}
                      onChange={(e) => handleChange('groupEntitiesByType', e.target.checked)}
                      className="form-checkbox h-4 w-4 text-blue-600"
                    />
                    <label htmlFor="group-entities" className="ml-2 text-sm text-gray-700">
                      Group entities by type
                    </label>
                  </div>
                  
                  <div className="flex items-center">
                    <label htmlFor="max-entities" className="text-sm text-gray-700">
                      Max entities to include:
                    </label>
                    <input
                      id="max-entities"
                      type="number"
                      value={options.maxEntities}
                      onChange={(e) => handleChange('maxEntities', parseInt(e.target.value, 10))}
                      className="ml-2 w-16 text-sm border rounded px-2 py-1"
                      min="10"
                      max="1000"
                    />
                  </div>
                  
                  <div className="mt-2 space-y-2 border-t border-gray-100 pt-2">
                    <h5 className="font-medium text-sm text-gray-700">Advanced NER Options</h5>
                    
                    <div className="flex items-center">
                      <input
                        id="include-relationships"
                        type="checkbox"
                        checked={options.includeEntityRelationships}
                        onChange={(e) => handleChange('includeEntityRelationships', e.target.checked)}
                        className="form-checkbox h-4 w-4 text-blue-600"
                      />
                      <label htmlFor="include-relationships" className="ml-2 text-sm text-gray-700">
                        Include entity relationships
                      </label>
                    </div>
                    
                    <div className="flex items-center">
                      <input
                        id="generate-graph"
                        type="checkbox"
                        checked={options.generateEntityGraph}
                        onChange={(e) => handleChange('generateEntityGraph', e.target.checked)}
                        className="form-checkbox h-4 w-4 text-blue-600"
                      />
                      <label htmlFor="generate-graph" className="ml-2 text-sm text-gray-700">
                        Generate entity graph visualization
                      </label>
                    </div>
                  </div>
                </div>
              )}
            </div>
            
            <div className="ml-4 mt-4 space-y-2 border-t pt-2">
              <h4 className="font-medium text-sm text-gray-800">Advanced Settings</h4>
              
              <div className="flex items-center">
                <input
                  id="use-tree-sitter"
                  type="checkbox"
                  checked={options.useTreeSitter}
                  onChange={(e) => handleChange('useTreeSitter', e.target.checked)}
                  className="form-checkbox h-4 w-4 text-blue-600"
                />
                <label htmlFor="use-tree-sitter" className="ml-2 text-sm text-gray-700">
                  Use Tree-sitter for better parsing (when available)
                </label>
              </div>
              
              <div className="flex items-center">
                <label htmlFor="file-size-threshold" className="text-sm text-gray-700">
                  File size threshold (bytes):
                </label>
                <input
                  id="file-size-threshold"
                  type="number"
                  value={options.fileSizeThreshold}
                  onChange={(e) => handleChange('fileSizeThreshold', parseInt(e.target.value, 10))}
                  className="ml-2 w-24 text-sm border rounded px-2 py-1"
                  min="0"
                  step="1024"
                />
              </div>
              
              <div className="flex items-center">
                <label htmlFor="max-summary-lines" className="text-sm text-gray-700">
                  Max summary lines:
                </label>
                <input
                  id="max-summary-lines"
                  type="number"
                  value={options.maxSummaryLines}
                  onChange={(e) => handleChange('maxSummaryLines', parseInt(e.target.value, 10))}
                  className="ml-2 w-16 text-sm border rounded px-2 py-1"
                  min="10"
                  max="1000"
                />
              </div>
            </div>
          </>
        )}
      </div>
    </div>
  );
};

export default SummarizationOptions; 