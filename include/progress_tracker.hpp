#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <chrono>
#include <iostream>
#include "file_processor.hpp"

/**
 * @brief Class for tracking progress of file processing jobs
 * 
 * This class provides a centralized way to track the progress of multiple
 * file processing jobs. It maintains a map of job IDs to progress information
 * and provides methods to register, update, and query job progress.
 */
class ProgressTracker {
public:
    // Structure to hold job information
    struct Job {
        std::string id;
        std::chrono::steady_clock::time_point startTime;
        FileProcessor::ProgressInfo lastProgress;
        bool isComplete = false;
    };
    
    // Singleton instance
    static ProgressTracker& getInstance() {
        static ProgressTracker instance;
        return instance;
    }
    
    // Register a new job
    std::string registerJob() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string jobId = generateJobId();
        
        Job job;
        job.id = jobId;
        job.startTime = std::chrono::steady_clock::now();
        
        jobs_[jobId] = job;
        return jobId;
    }
    
    // Update job progress
    void updateProgress(const std::string& jobId, const FileProcessor::ProgressInfo& progress) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = jobs_.find(jobId);
        if (it != jobs_.end()) {
            it->second.lastProgress = progress;
            it->second.isComplete = progress.isComplete;
            
            // Log progress to console
            std::cout << "[Job " << jobId << "] Progress: " 
                      << progress.getPercentage() << "% ("
                      << progress.processedFiles << "/" << progress.totalFiles 
                      << " files)" << std::endl;
            
            // If job is complete, log completion
            if (progress.isComplete) {
                auto now = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - it->second.startTime).count();
                
                std::cout << "[Job " << jobId << "] Completed in " 
                          << duration << "ms" << std::endl;
            }
        }
    }
    
    // Get job progress
    bool getJobProgress(const std::string& jobId, Job& job) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = jobs_.find(jobId);
        if (it != jobs_.end()) {
            job = it->second;
            return true;
        }
        
        return false;
    }
    
    // Get all jobs
    std::unordered_map<std::string, Job> getAllJobs() {
        std::lock_guard<std::mutex> lock(mutex_);
        return jobs_;
    }
    
    // Remove a job
    void removeJob(const std::string& jobId) {
        std::lock_guard<std::mutex> lock(mutex_);
        jobs_.erase(jobId);
    }
    
    // Clean up completed jobs older than specified duration
    void cleanupCompletedJobs(std::chrono::milliseconds olderThan) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::steady_clock::now();
        
        for (auto it = jobs_.begin(); it != jobs_.end(); ) {
            if (it->second.isComplete) {
                auto jobAge = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - it->second.startTime);
                
                if (jobAge > olderThan) {
                    it = jobs_.erase(it);
                } else {
                    ++it;
                }
            } else {
                ++it;
            }
        }
    }
    
private:
    // Private constructor for singleton
    ProgressTracker() = default;
    
    // Generate a unique job ID
    std::string generateJobId() {
        auto timestamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        return "job_" + timestamp;
    }
    
    // Map of job IDs to job information
    std::unordered_map<std::string, Job> jobs_;
    
    // Mutex for thread safety
    std::mutex mutex_;
}; 