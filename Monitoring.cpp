#include "Monitoring.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <cstring>
#include <sys/inotify.h>
#include <iostream>
#include <errno.h>

using namespace std;

namespace fs = std::filesystem;

void addFileToWatch(int inotifyFd, const string& filePath, 
                    unordered_set<string>& trackedFiles, 
                    unordered_map<int, string>& watchDescriptors, 
                    mutex& mtx) {
    lock_guard<mutex> lock(mtx);
    if (trackedFiles.find(filePath) != trackedFiles.end()) {
        cout << "File is already being tracked: " << filePath << endl;
        return;
    }

    int wd = inotify_add_watch(inotifyFd, filePath.c_str(), IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_TO);
    if (wd == -1) {
        cerr << "Error adding file to watch: " << filePath << " - ";
        perror("inotify_add_watch");
        return;
    }

    trackedFiles.insert(filePath);
    watchDescriptors[wd] = filePath;
    cout << "Added file to track: " << filePath << " (wd: " << wd << ")" << endl;
}

void removeFileFromWatch(int inotifyFd, const string& filePath, 
                         unordered_set<string>& trackedFiles, 
                         unordered_map<int, string>& watchDescriptors, 
                         bool isMonitoring) {
    cout << "Debug: Entering removeFileFromWatch for file: " << filePath << endl;
    cout << "Debug: isMonitoring = " << (isMonitoring ? "true" : "false") << ", inotifyFd = " << inotifyFd << endl;

    auto it = trackedFiles.find(filePath);
    if (it == trackedFiles.end()) {
        cout << "Error: File not found in tracking list: " << filePath << endl;
        return;
    }

    int wdToRemove = -1;
    for (const auto& pair : watchDescriptors) {
        if (pair.second == filePath) {
            wdToRemove = pair.first;
            break;
        }
    }

    if (wdToRemove != -1) {
        cout << "Debug: Found watch descriptor " << wdToRemove << " for file: " << filePath << endl;
        if (isMonitoring && inotifyFd != -1) { // Only attempt to remove watch if monitoring is active and inotifyFd is valid
            cout << "Debug: Attempting inotify_rm_watch for wd: " << wdToRemove << endl;
            int ret = inotify_rm_watch(inotifyFd, wdToRemove);
            if (ret == -1) {
                cerr << "Error removing watch descriptor " << wdToRemove << " for file: " << filePath << " - ";
                perror("inotify_rm_watch");
            } else {
                cout << "Debug: Successfully removed watch descriptor " << wdToRemove << " for file: " << filePath << endl;
            }
        } else {
            cout << "Debug: Monitoring is not active or inotifyFd is invalid, skipping inotify_rm_watch for file: " << filePath << endl;
        }
        watchDescriptors.erase(wdToRemove);
        cout << "Debug: Removed watch descriptor from watchDescriptors" << endl;
    } else {
        cerr << "Could not find watch descriptor for file: " << filePath << endl;
    }

    trackedFiles.erase(it);
    cout << "Debug: Removed file from trackedFiles: " << filePath << endl;
    cout << "Debug: Exiting removeFileFromWatch" << endl;
}

void startMonitoringThread(bool& isMonitoring, thread& monitoringThread, 
                          int inotifyFd, const unordered_set<string>& trackedFiles, 
                          const unordered_map<int, string>& watchDescriptors, 
                          mutex& mtx) {
    if (isMonitoring) {
        cout << "Monitoring is already running!" << endl;
        return;
    }

    isMonitoring = true;
    monitoringThread = thread([inotifyFd, &trackedFiles, &watchDescriptors, &mtx, &isMonitoring]() {
        const size_t EVENT_SIZE = sizeof(struct inotify_event);
        const size_t BUF_LEN = 1024 * (EVENT_SIZE + 16);
        char buffer[BUF_LEN];

        ofstream log("file_monitor.log", ios::app);
        if (!log.is_open()) {
            cerr << "Failed to open file_monitor.log" << endl;
            isMonitoring = false;
            return;
        }

        while (isMonitoring) {
            ssize_t length = read(inotifyFd, buffer, BUF_LEN);
            if (length < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    usleep(100000); // Sleep for 100ms
                    continue;
                } else if (errno == EBADF) {
                    log << "inotify file descriptor closed, stopping monitoring thread." << endl;
                    break;
                } else {
                    log << "Error reading inotify: " << strerror(errno) << endl;
                    break;
                }
            }

            if (length == 0) {
                log << "Read returned 0, stopping monitoring thread." << endl;
                break;
            }

            for (char* ptr = buffer; ptr < buffer + length; ) {
                struct inotify_event* event = reinterpret_cast<struct inotify_event*>(ptr);
                if (event->mask & (IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_TO)) {
                    lock_guard<mutex> lock(mtx);
                    auto it = watchDescriptors.find(event->wd);
                    if (it != watchDescriptors.end()) {
                        const string& filePath = it->second;
                        auto now = chrono::system_clock::now();
                        auto now_time = chrono::system_clock::to_time_t(now);
                        tm now_tm = *localtime(&now_time);
                        ostringstream timestamp;
                        timestamp << put_time(&now_tm, "%Y-%m-%d %H:%M:%S");

                        fs::path src(filePath);
                        fs::path dest = "backups/" + src.filename().string() + "_" + timestamp.str();
                        try {
                            if (!fs::exists(filePath)) {
                                log << timestamp.str() << ": File does not exist: " << filePath << endl;
                                continue;
                            }
                            if (!fs::exists("backups")) {
                                fs::create_directory("backups");
                                log << timestamp.str() << ": Created backups directory" << endl;
                            }
                            fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
                            log << timestamp.str() << ": Created backup: " << dest << endl;

                            ofstream changeLog("changes.log", ios::app);
                            if (!changeLog.is_open()) {
                                log << timestamp.str() << ": Failed to open changes.log: " << strerror(errno) << endl;
                                continue;
                            }
                            changeLog << timestamp.str() << ": " << filePath << " (backup: " << dest << ")" << endl;
                            changeLog.close();
                            log << timestamp.str() << ": Logged change to changes.log: " << filePath << endl;
                        } catch (const fs::filesystem_error& e) {
                            log << timestamp.str() << ": Error during backup or logging: " << e.what() << endl;
                        }
                    } else {
                        log << "Watch descriptor not found: " << event->wd << endl;
                    }
                }
                ptr += EVENT_SIZE + event->len;
            }
        }
        log.close();
        cout << "Monitoring thread exited." << endl;
    });
    cout << "File monitoring started." << endl;
}

void stopMonitoringThread(bool& isMonitoring, thread& monitoringThread) {
    if (!isMonitoring) return;

    isMonitoring = false;
    if (monitoringThread.joinable()) {
        monitoringThread.join();
        cout << "Monitoring thread joined successfully." << endl;
    } else {
        cout << "Monitoring thread not joinable." << endl;
    }
    cout << "File monitoring stopped." << endl;
}

void listTrackedFilesImpl(const unordered_set<string>& trackedFiles, 
                         mutex& mtx) {
    lock_guard<mutex> lock(mtx);
    if (trackedFiles.empty()) {
        cout << "No files are being tracked." << endl;
        return;
    }

    cout << "Tracked files:" << endl;
    for (const auto& file : trackedFiles) {
        cout << " - " << file << endl;
    }
}
