#include "UI.h"
#include "Utils.h"
#include "Navigation.h"
#include <filesystem>
#include <dirent.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <locale.h>
#include <iostream>
#include <unistd.h> // Added for sleep function

using namespace std;

namespace fs = std::filesystem;

// Initializes color pairs for the ncurses interface
void initColors() {
    if (!has_colors()) {
        return;
    }
    start_color();
    use_default_colors();
    init_pair(COLOR_DIR, COLOR_GREEN, -1);
    init_pair(COLOR_FILE, COLOR_WHITE, -1);
    init_pair(COLOR_LINK, COLOR_CYAN, -1);
    init_pair(COLOR_HEADER, COLOR_YELLOW, -1);
    init_pair(COLOR_HIGHLIGHT, COLOR_BLACK, COLOR_WHITE);
}

// Prints text with the specified color and highlight
void printColored(WINDOW* win, const string& text, int colorPair, bool highlight) {
    if (highlight) {
        wattron(win, COLOR_PAIR(COLOR_HIGHLIGHT));
    } else {
        wattron(win, COLOR_PAIR(colorPair));
    }
    wprintw(win, "%s", text.c_str());
    wattroff(win, COLOR_PAIR(colorPair));
    if (highlight) {
        wattroff(win, COLOR_PAIR(COLOR_HIGHLIGHT));
    }
}

// Implements the file selection interface using ncurses
string browseAndSelectFileImpl(const string& startDir, 
                              deque<string>& dirHistory, 
                              stack<string>& backStack) {
    // Set locale for UTF-8 support (kept in case the terminal supports it)
    setlocale(LC_ALL, "");

    fflush(stdout);
    fflush(stderr);

    initscr();
    start_color();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    initColors();

    if (has_colors() == FALSE) {
        endwin();
        cerr << "Your terminal does not support colors. Exiting.\n";
        return "";
    }

    string currentDir = startDir;
    char filter = 0;
    size_t page = 0;
    size_t itemsPerPage = 10; // Made variable to allow dynamic changes
    size_t selectedItem = 0;
    bool refreshNeeded = true;
    vector<pair<string, char>> files;
    size_t startIdx = 0;

    while (true) {
        if (refreshNeeded) {
            clear();
            // Header (increased length to 80 characters + 2 for borders)
            printColored(stdscr, "+--------------------------------------------------------------------------------+\n", COLOR_HEADER);
            printColored(stdscr, "| Select file to track (q - exit)                                                      |\n", COLOR_HEADER);
            string dirLine = "| Current directory: " + fs::absolute(currentDir).string();
            dirLine.resize(80, ' ');
            dirLine += "|\n";
            printColored(stdscr, dirLine, COLOR_HEADER);
            printColored(stdscr, "+--------------------------------------------------------------------------------+\n", COLOR_HEADER);

            // File list
            files.clear();
            DIR* dir = opendir(currentDir.c_str());
            if (dir) {
                struct dirent* entry;
                while ((entry = readdir(dir)) != nullptr) {
                    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                        continue;

                    string fullPath = currentDir + "/" + entry->d_name;
                    char type = entry->d_type;
                    if (type == DT_UNKNOWN) {
                        if (fs::is_directory(fullPath)) type = DT_DIR;
                        else if (fs::is_symlink(fullPath)) type = DT_LNK;
                        else if (fs::is_regular_file(fullPath)) type = DT_REG;
                    }
                    if (filter == 0 || 
                        (filter == 'f' && type == DT_REG) ||
                        (filter == 'd' && type == DT_DIR) ||
                        (filter == 'l' && type == DT_LNK)) {
                        files.emplace_back(fullPath, type);
                    }
                }
                closedir(dir);
            }

            sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
                if (a.second == DT_DIR && b.second != DT_DIR) return true;
                if (a.second != DT_DIR && b.second == DT_DIR) return false;
                return a.first < b.first;
            });

            size_t totalPages = (files.size() + itemsPerPage - 1) / itemsPerPage;
            startIdx = page * itemsPerPage;
            size_t endIdx = min(startIdx + itemsPerPage, files.size());

            // Page information
            string pageInfo = "Page " + to_string(page + 1) + " of " + to_string(totalPages);
            printColored(stdscr, pageInfo + "\n\n", COLOR_HEADER);

            // Display file list
            for (size_t i = startIdx; i < endIdx; i++) {
                bool isSelected = (i - startIdx == selectedItem);
                string entry = "  " + to_string(i - startIdx + 1) + ". ";
                int color;
                switch (files[i].second) {
                    case DT_DIR: color = COLOR_DIR; entry += "[DIR] "; break;
                    case DT_REG: color = COLOR_FILE; entry += "[FILE] "; break;
                    case DT_LNK: color = COLOR_LINK; entry += "[LINK] "; break;
                    default: color = COLOR_FILE; entry += "[?] ";
                }
                entry += fs::path(files[i].first).filename().string();
                printColored(stdscr, entry + "\n", color, isSelected);
            }

            // Controls (increased length to 80 characters + 2 for borders)
            printColored(stdscr, "\n+--------------------------------------------------------------------------------+\n", COLOR_HEADER);
            printColored(stdscr, "| Controls:                                                                      |\n", COLOR_HEADER);
            printColored(stdscr, "| Arrows: navigate   Enter: select   q: exit   Home: go to home directory       |\n", COLOR_HEADER);
            printColored(stdscr, "| f/d/l/a: filters   +/-: page size   h: history   p: previous                  |\n", COLOR_HEADER);
            printColored(stdscr, "+--------------------------------------------------------------------------------+\n", COLOR_HEADER);

            refresh();
            refreshNeeded = false;
        }

        int ch = getch();
        refreshNeeded = true;

        switch(ch) {
            case KEY_UP:
                if (selectedItem > 0) selectedItem--;
                break;
            case KEY_DOWN:
                if (selectedItem < min(itemsPerPage, files.size() - startIdx) - 1) selectedItem++;
                break;
            case KEY_LEFT:
                if (page > 0) {
                    page--;
                    selectedItem = 0;
                }
                break;
            case KEY_RIGHT:
                if (page < (files.size() + itemsPerPage - 1) / itemsPerPage - 1) {
                    page++;
                    selectedItem = 0;
                }
                break;
            case KEY_HOME:
                currentDir = getenv("HOME") ? getenv("HOME") : "/";
                page = 0;
                selectedItem = 0;
                break;
            case KEY_BACKSPACE:
                if (!currentDir.empty()) {
                    backStack.push(currentDir);
                    currentDir = fs::path(currentDir).parent_path().string();
                    if (currentDir.empty()) currentDir = "/";
                    page = 0;
                    selectedItem = 0;
                }
                break;
            case 'h':
                showHistory(dirHistory);
                break;
            case '+':
                if (itemsPerPage < 50) {
                    itemsPerPage += 5;
                    page = 0;
                    selectedItem = 0;
                }
                break;
            case '-':
                if (itemsPerPage > 5) {
                    itemsPerPage -= 5;
                    page = 0;
                    selectedItem = 0;
                }
                break;
            case 'p':
                {
                    string parentDir = fs::path(currentDir).parent_path().string();
                    if (parentDir != currentDir) { // Avoid infinite loop at root
                        currentDir = parentDir;
                        page = 0;
                        selectedItem = 0;
                    } else {
                        printw("Already at root directory.\n");
                        refresh();
                        sleep(1); // Brief delay to show message
                    }
                }
                break;
            case 'f': case 'd': case 'l': case 'a':
                filter = (ch == 'a') ? 0 : ch;
                page = 0;
                selectedItem = 0;
                break;
            case 'q':
                endwin();
                return "";
            case '\n':
                if (!files.empty() && selectedItem < files.size() - startIdx) {
                    size_t idx = startIdx + selectedItem;
                    if (files[idx].second == DT_DIR) {
                        currentDir = files[idx].first;
                        page = 0;
                        selectedItem = 0;
                    } else {
                        endwin();
                        return files[idx].first;
                    }
                }
                break;
        }
    }
    endwin();
    return "";
}
