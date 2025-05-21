#include "FileMonitor.h"
#include "Utils.h" // Added for clearScreen
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <limits>
#include <sstream>
#include <fstream>
#include <sys/stat.h>

using namespace std;

// Проверяет, существует ли другой процесс мониторинга
bool isAnotherInstanceRunning(pid_t& existingPid) {
    ifstream lockFile("file_monitor.lock");
    if (!lockFile.is_open()) {
        return false; // Файл блокировки не существует, значит процесс не запущен
    }

    pid_t pid;
    lockFile >> pid;
    lockFile.close();

    // Проверяем, существует ли процесс с этим PID
    if (kill(pid, 0) == 0) {
        existingPid = pid;
        return true; // Процесс существует
    } else {
        // Процесс не существует, удаляем файл блокировки
        remove("file_monitor.lock");
        return false;
    }
}

// Создаёт файл блокировки с текущим PID
void createLockFile() {
    ofstream lockFile("file_monitor.lock");
    if (!lockFile.is_open()) {
        cerr << "Failed to create lock file!" << endl;
        return;
    }
    lockFile << getpid() << endl;
    lockFile.close();
}

int main(int argc, char* argv[]) {
    clearScreen();
    FileMonitor monitor;
    bool background = false;

    // Check for --background flag
    for (int i = 1; i < argc; ++i) {
        if (string(argv[i]) == "--background") {
            background = true;
            break;
        }
    }

    if (background) {
        // Ignore SIGHUP to prevent termination when terminal closes
        signal(SIGHUP, SIG_IGN);

        // Detach from terminal
        setsid();

        // Redirect standard file descriptors
        close(0); // Close stdin
        open("/dev/null", O_RDONLY); // Reopen stdin to /dev/null
        freopen("file_monitor.log", "a", stdout);
        freopen("file_monitor.err", "a", stderr);

        // Создаём файл блокировки перед началом мониторинга
        createLockFile();
        monitor.startMonitoring();
        while (true) {
            sleep(60); // Keep process alive
        }
    } else {
        // Interactive mode with ncurses interface
        string input;
        int choice;
        do {
            cout << "1. Add file to track" << endl;
            cout << "2. Remove file from tracking" << endl;
            cout << "3. Start monitoring" << endl;
            cout << "4. Stop monitoring" << endl;
            cout << "5. List tracked files" << endl;
            cout << "6. Switch to background mode or exit" << endl;
            cout << "Enter choice (1-6): ";

            // Read input as string to handle invalid input
            getline(cin, input);
            istringstream iss(input);

            // Check if the input can be converted to an integer
            if (!(iss >> choice)) {
                cout << "Invalid input: please enter a number between 1 and 6." << endl;
                cin.clear(); // Clear error flags
                cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignore remaining input
                continue;
            }

            // Check if there is extra input after the number
            string remaining;
            if (getline(iss, remaining) && !remaining.empty() && remaining.find_first_not_of(" \t") != string::npos) {
                cout << "Invalid input: extra characters after the number." << endl;
                cin.clear(); // Clear error flags
                cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignore remaining input
                continue;
            }

            // Check if the number is within the valid range for int and menu options
            if (choice < 1 || choice > 6) {
                cout << "Invalid choice: please enter a number between 1 and 6." << endl;
                cin.clear(); // Clear error flags
                cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Ignore remaining input
                continue;
            }

            string filePath;
            switch (choice) {
                case 1:
                    clearScreen(); // Clear the terminal before adding a file
                    filePath = monitor.browseAndSelectFile();
                    if (!filePath.empty()) {
                        monitor.addFile(filePath);
                    }
                    break;
                case 2:
                    clearScreen(); // Clear the terminal before removing a file
                    monitor.removeFileByIndex();
                    break;
                case 3: {
                    pid_t existingPid;
                    if (isAnotherInstanceRunning(existingPid)) {
                        cout << "Monitoring is already running in background (PID: " << existingPid << ")." << endl;
                    } else {
                        createLockFile();
                        monitor.startMonitoring();
                    }
                    break;
                }
                case 4:
                    monitor.stopMonitoring();
                    cout << "Monitoring stopped." << endl;
                    remove("file_monitor.lock"); // Удаляем файл блокировки при остановке
                    break;
                case 5:
                    clearScreen(); // Clear the terminal before listing files
                    monitor.listTrackedFiles();
                    cout << "Press Enter to continue...\n";
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    break;
                case 6: { // Switch to background mode or exit
                    if (!monitor.isMonitoringActive()) {
                        cout << "Monitoring is not active. Exiting program..." << endl;
                        exit(0); // Завершаем программу, если мониторинг не активен
                    }
                    cout << "Switching to background mode..." << endl;
                    pid_t pid = fork();
                    if (pid < 0) {
                        cerr << "Fork failed!" << endl;
                        exit(1);
                    } else if (pid == 0) { // Child process
                        // Ignore SIGHUP to prevent termination when terminal closes
                        signal(SIGHUP, SIG_IGN);

                        // Detach from terminal
                        setsid();

                        // Redirect standard file descriptors
                        close(0); // Close stdin
                        open("/dev/null", O_RDONLY); // Reopen stdin to /dev/null
                        freopen("file_monitor.log", "a", stdout);
                        freopen("file_monitor.err", "a", stderr);

                        // Reinitialize FileMonitor in child process
                        FileMonitor backgroundMonitor;
                        // Создаём файл блокировки перед началом мониторинга
                        createLockFile();
                        backgroundMonitor.startMonitoring();
                        while (true) {
                            sleep(60); // Keep process alive in background
                        }
                    } else { // Parent process
                        cout << "Switched to background mode. Parent process exiting..." << endl;
                        exit(0); // Parent exits, allowing terminal to close
                    }
                    break;
                }
                default:
                    cout << "Invalid choice" << endl;
            }
            cout << "Debug: Processed choice " << choice << endl; // Debug output
        } while (true); // Infinite loop until background mode or exit is chosen
    }

    return 0;
}
