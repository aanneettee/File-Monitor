#ifndef MONITORING_H
#define MONITORING_H

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <thread>
#include <mutex>

void addFileToWatch(int inotifyFd, const std::string& filePath, 
                    std::unordered_set<std::string>& trackedFiles, 
                    std::unordered_map<int, std::string>& watchDescriptors, 
                    std::mutex& mtx);

void removeFileFromWatch(int inotifyFd, const std::string& filePath, 
                         std::unordered_set<std::string>& trackedFiles, 
                         std::unordered_map<int, std::string>& watchDescriptors, 
                         bool isMonitoring);

void startMonitoringThread(bool& isMonitoring, std::thread& monitoringThread, 
                          int inotifyFd, const std::unordered_set<std::string>& trackedFiles, 
                          const std::unordered_map<int, std::string>& watchDescriptors, 
                          std::mutex& mtx);

void stopMonitoringThread(bool& isMonitoring, std::thread& monitoringThread);

void listTrackedFilesImpl(const std::unordered_set<std::string>& trackedFiles, 
                         std::mutex& mtx);

#endif
