#ifndef FILE_MONITOR_H
#define FILE_MONITOR_H

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <deque>
#include <stack>

class FileMonitor {
public:
    FileMonitor();
    ~FileMonitor();

    void addFile(const std::string& filePath);
    void removeFile(const std::string& filePath);
    void removeFileByIndex();
    void startMonitoring();
    void stopMonitoring();
    void listTrackedFiles() const;
    std::string browseAndSelectFile(const std::string& startDir = ".") const;

    // Новый метод для проверки состояния мониторинга
    bool isMonitoringActive() const {
        return isMonitoring;
    }

private:
    int inotifyFd;
    std::unordered_set<std::string> trackedFiles;
    std::unordered_map<int, std::string> watchDescriptors;
    bool isMonitoring;
    std::thread monitoringThread;
    mutable std::mutex mtx;
    mutable std::deque<std::string> dirHistory;
    mutable std::stack<std::string> backStack;

    void loadTrackedFiles();
    void saveTrackedFiles();
};

#endif
