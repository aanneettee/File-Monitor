#include "FileMonitor.h"
#include "Monitoring.h"
#include "UI.h"
#include "Navigation.h"
#include "Utils.h"
#include <filesystem>
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <fstream>
#include <cstring>
#include <vector>
#include <sstream>

using namespace std;

namespace fs = std::filesystem;

// Constructor: Initializes inotify and creates backups directory
FileMonitor::FileMonitor() : inotifyFd(-1), isMonitoring(false) {
    inotifyFd = inotify_init();
    if (inotifyFd == -1) {
        perror("inotify_init");
        exit(EXIT_FAILURE);
    }
    // Set inotifyFd to non-blocking mode
    int flags = fcntl(inotifyFd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl get flags");
        exit(EXIT_FAILURE);
    }
    if (fcntl(inotifyFd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl set non-blocking");
        exit(EXIT_FAILURE);
    }
    fs::create_directory("backups");
    loadTrackedFiles(); // Load existing tracked files on startup
}

// Destructor: Stops monitoring and saves tracked files
FileMonitor::~FileMonitor() {
    saveTrackedFiles(); // Save tracked files before exit
    stopMonitoring();
    if (inotifyFd != -1) {
        close(inotifyFd);
    }
    endwin(); // Ensure ncurses is properly terminated
}

// Load tracked files from file
void FileMonitor::loadTrackedFiles() {
    ifstream in("tracked_files.txt");
    string filePath;
    while (getline(in, filePath)) {
        if (!filePath.empty()) {
            addFile(filePath);
        }
    }
    in.close();
    cout << "Loaded " << trackedFiles.size() << " files from tracked_files.txt" << endl;
}

// Save tracked files to file
void FileMonitor::saveTrackedFiles() {
    lock_guard<mutex> lock(mtx);
    cout << "Saving " << trackedFiles.size() << " tracked files to tracked_files.txt" << endl;
    ofstream out("tracked_files.txt");
    if (!out.is_open()) {
        cerr << "Failed to open tracked_files.txt for writing: " << strerror(errno) << endl;
        return;
    }
    for (const auto& file : trackedFiles) {
        out << file << endl;
        cout << "Saved file: " << file << endl;
    }
    out.close();
}

// Adds a file to the tracking list
void FileMonitor::addFile(const string& filePath) {
    addFileToWatch(inotifyFd, filePath, trackedFiles, watchDescriptors, mtx);
}

// Removes a file from the tracking list
void FileMonitor::removeFile(const string& filePath) {
    removeFileFromWatch(inotifyFd, filePath, trackedFiles, watchDescriptors, isMonitoring);
}

// Removes a file by index (interactive mode)
void FileMonitor::removeFileByIndex() {
    lock_guard<mutex> lock(mtx);
    
    // Clear the terminal
    clearScreen();

    // Check if there are any tracked files
    if (trackedFiles.empty()) {
        cout << "+------------------------------------------+\n";
        cout << "| No files are being tracked.              |\n";
        cout << "+------------------------------------------+\n";
        cout << "Press Enter to continue...\n";
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return;
    }

    // Display the list of tracked files with numbers
    vector<string> fileList(trackedFiles.begin(), trackedFiles.end());
    cout << "+------------------------------------------+\n";
    cout << "| Select a file to remove:                 |\n";
    cout << "+------------------------------------------+\n";
    
    const size_t maxDisplayLength = 36; // Maximum length for display
    for (size_t i = 0; i < fileList.size(); ++i) {
        string displayName = fs::path(fileList[i]).filename().string();
        if (displayName.length() > maxDisplayLength) {
            displayName = displayName.substr(0, maxDisplayLength - 3) + "...";
        }
        size_t paddingLength = maxDisplayLength - displayName.length();
        string padding = (paddingLength > 0) ? string(paddingLength, ' ') : "";
        cout << "| " << (i + 1) << ". " << displayName << padding << " |\n";
    }
    cout << "+------------------------------------------+\n";
    cout << "| Enter the number of the file to remove   |\n";
    cout << "| (or 0 to cancel):                        |\n";
    cout << "+------------------------------------------+\n";
    cout << "Choice: ";

    // Read user input
    string input;
    getline(cin, input);
    istringstream iss(input);
    int choice;
    
    // Validate input
    if (!(iss >> choice)) {
        cout << "Invalid input: please enter a number.\n";
        cout << "Press Enter to continue...\n";
        cin.clear(); // Clear error flags
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return;
    }

    // Check for extra input after the number
    string remaining;
    if (getline(iss, remaining) && !remaining.empty() && remaining.find_first_not_of(" \t") != string::npos) {
        cout << "Invalid input: extra characters after the number.\n";
        cout << "Press Enter to continue...\n";
        cin.clear(); // Clear error flags
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return;
    }

    // Handle the choice
    if (choice == 0) {
        cout << "Removal cancelled.\n";
        cout << "Press Enter to continue...\n";
        cin.clear(); // Clear error flags
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return;
    }

    if (choice < 1 || choice > static_cast<int>(fileList.size())) {
        cout << "Invalid choice: please select a number between 1 and " << fileList.size() << ".\n";
        cout << "Press Enter to continue...\n";
        cin.clear(); // Clear error flags
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return;
    }

    // Remove the selected file
    string fileToRemove = fileList[choice - 1];
    cout << "Debug: Attempting to remove file: " << fileToRemove << endl;
    removeFileFromWatch(inotifyFd, fileToRemove, trackedFiles, watchDescriptors, isMonitoring);
    cout << "File removed: " << fileToRemove << "\n";
    cout << "Press Enter to continue...\n";
    cin.clear(); // Clear error flags
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cout << "Debug: After ignore, returning to menu" << endl;
}

// Starts the monitoring thread
void FileMonitor::startMonitoring() {
    startMonitoringThread(isMonitoring, monitoringThread, inotifyFd, trackedFiles, watchDescriptors, mtx);
}

// Stops the monitoring thread
void FileMonitor::stopMonitoring() {
    cout << "Stopping monitoring..." << endl;
    if (inotifyFd != -1) {
        close(inotifyFd);
        inotifyFd = -1;
    }
    stopMonitoringThread(isMonitoring, monitoringThread);
    cout << "Monitoring stopped in FileMonitor." << endl;
}

// Lists all tracked files
void FileMonitor::listTrackedFiles() const {
    listTrackedFilesImpl(trackedFiles, mtx);
}

// Opens a file selection interface and returns the selected file path
string FileMonitor::browseAndSelectFile(const string& startDir) const {
    return browseAndSelectFileImpl(startDir, dirHistory, backStack);
}
