import React, { useState } from 'react';

interface FileScoringResultsProps {
  scoringReport: any;  // The scoring report returned from the API
}

const FileScoringResults: React.FC<FileScoringResultsProps> = ({ scoringReport }) => {
  const [sortField, setSortField] = useState<string>('score');
  const [sortDirection, setSortDirection] = useState<'asc' | 'desc'>('desc');
  const [showDetails, setShowDetails] = useState<{ [key: string]: boolean }>({});

  if (!scoringReport || !scoringReport.files) {
    return null;
  }

  const { summary, files } = scoringReport;
  
  // Sort the files based on the selected field and direction
  const sortedFiles = [...files].sort((a, b) => {
    let aValue = a[sortField];
    let bValue = b[sortField];
    
    // For component scores, default to 0 if the component doesn't exist
    if (sortField.startsWith('components.')) {
      const component = sortField.split('.')[1];
      aValue = a.components && a.components[component] ? a.components[component] : 0;
      bValue = b.components && b.components[component] ? b.components[component] : 0;
    }
    
    if (aValue === bValue) return 0;
    
    const comparison = aValue < bValue ? -1 : 1;
    return sortDirection === 'asc' ? comparison : -comparison;
  });

  const handleSort = (field: string) => {
    if (sortField === field) {
      // Toggle direction if the same field is clicked
      setSortDirection(sortDirection === 'asc' ? 'desc' : 'asc');
    } else {
      // Set new field and default to descending
      setSortField(field);
      setSortDirection('desc');
    }
  };

  const toggleDetails = (path: string) => {
    setShowDetails(prev => ({
      ...prev,
      [path]: !prev[path]
    }));
  };

  // Get a color class based on the score (green for high, yellow for medium, red for low)
  const getScoreColorClass = (score: number) => {
    if (score >= 0.7) return 'text-green-600';
    if (score >= 0.4) return 'text-yellow-600';
    return 'text-red-600';
  };

  return (
    <div className="space-y-4">
      <div className="bg-white shadow rounded-lg p-4">
        <h3 className="text-lg font-medium text-gray-900 mb-2">File Scoring Summary</h3>
        <div className="grid grid-cols-3 gap-4">
          <div className="bg-gray-50 p-3 rounded">
            <p className="text-sm text-gray-500">Total Files</p>
            <p className="text-xl font-bold">{summary.total_files}</p>
          </div>
          <div className="bg-gray-50 p-3 rounded">
            <p className="text-sm text-gray-500">Included Files</p>
            <p className="text-xl font-bold">{summary.included_files}</p>
          </div>
          <div className="bg-gray-50 p-3 rounded">
            <p className="text-sm text-gray-500">Inclusion Rate</p>
            <p className="text-xl font-bold">{summary.inclusion_percentage.toFixed(1)}%</p>
          </div>
        </div>
      </div>
      
      <div className="bg-white shadow rounded-lg overflow-hidden">
        <div className="px-4 py-3 border-b">
          <h3 className="text-lg font-medium text-gray-900">Scored Files</h3>
          <p className="text-sm text-gray-500">Showing top {files.length} files by score</p>
        </div>
        
        <div className="overflow-x-auto">
          <table className="min-w-full divide-y divide-gray-200">
            <thead className="bg-gray-50">
              <tr>
                <th 
                  scope="col" 
                  className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider cursor-pointer"
                  onClick={() => handleSort('path')}
                >
                  File Path
                  {sortField === 'path' && (
                    <span className="ml-1">{sortDirection === 'asc' ? '↑' : '↓'}</span>
                  )}
                </th>
                <th 
                  scope="col" 
                  className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider cursor-pointer"
                  onClick={() => handleSort('score')}
                >
                  Score
                  {sortField === 'score' && (
                    <span className="ml-1">{sortDirection === 'asc' ? '↑' : '↓'}</span>
                  )}
                </th>
                <th 
                  scope="col" 
                  className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider cursor-pointer"
                  onClick={() => handleSort('included')}
                >
                  Status
                  {sortField === 'included' && (
                    <span className="ml-1">{sortDirection === 'asc' ? '↑' : '↓'}</span>
                  )}
                </th>
                <th scope="col" className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase tracking-wider">
                  Details
                </th>
              </tr>
            </thead>
            <tbody className="bg-white divide-y divide-gray-200">
              {sortedFiles.map((file: any) => (
                <React.Fragment key={file.path}>
                  <tr className="hover:bg-gray-50">
                    <td className="px-6 py-4 whitespace-nowrap text-sm font-medium text-gray-900 max-w-xs truncate">
                      {file.path}
                    </td>
                    <td className="px-6 py-4 whitespace-nowrap text-sm">
                      <span className={`font-bold ${getScoreColorClass(file.score)}`}>
                        {(file.score * 100).toFixed(1)}%
                      </span>
                    </td>
                    <td className="px-6 py-4 whitespace-nowrap text-sm">
                      {file.included ? (
                        <span className="px-2 inline-flex text-xs leading-5 font-semibold rounded-full bg-green-100 text-green-800">
                          Included
                        </span>
                      ) : (
                        <span className="px-2 inline-flex text-xs leading-5 font-semibold rounded-full bg-red-100 text-red-800">
                          Excluded
                        </span>
                      )}
                    </td>
                    <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
                      <button
                        onClick={() => toggleDetails(file.path)}
                        className="text-blue-600 hover:text-blue-800"
                      >
                        {showDetails[file.path] ? 'Hide' : 'Show'} Details
                      </button>
                    </td>
                  </tr>
                  {showDetails[file.path] && (
                    <tr className="bg-gray-50">
                      <td colSpan={4} className="px-6 py-4">
                        <div className="text-sm">
                          <h4 className="font-medium text-gray-900 mb-2">Score Components</h4>
                          <div className="grid grid-cols-2 md:grid-cols-3 gap-2">
                            {file.components && Object.entries(file.components).map(([component, value]: [string, any]) => (
                              <div key={component} className="bg-white p-2 rounded border">
                                <p className="text-xs text-gray-500 capitalize">{component.replace('_', ' ')}</p>
                                <p className="font-medium">{(value * 100).toFixed(1)}%</p>
                              </div>
                            ))}
                          </div>
                        </div>
                      </td>
                    </tr>
                  )}
                </React.Fragment>
              ))}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  );
};

export default FileScoringResults; 