#include "Navigation.h"
#include "Utils.h"
#include <filesystem>
#include <iostream>

using namespace std;

namespace fs = std::filesystem;

// Shows the navigation history in a window
void showHistory(const deque<string>& dirHistory) {
    const int MAX_HISTORY_ITEMS = 50;
    WINDOW* histWin = newwin(15, 70, (LINES-15)/2, (COLS-70)/2);
    box(histWin, 0, 0);
    mvwprintw(histWin, 1, 1, "Navigation History (last %d entries):", MAX_HISTORY_ITEMS);
    
    int line = 3;
    size_t startIdx = dirHistory.size() > MAX_HISTORY_ITEMS ? dirHistory.size() - MAX_HISTORY_ITEMS : 0;
    for (size_t i = startIdx; i < dirHistory.size(); i++) {
        mvwprintw(histWin, line++, 1, "%zu. %s", i+1, dirHistory[i].c_str());
        if (line > 13) break;
    }
    
    mvwprintw(histWin, 13, 1, "Press any key to continue...");
    wrefresh(histWin);
    wgetch(histWin);
    delwin(histWin);
}
