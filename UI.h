#ifndef UI_H
#define UI_H

#include <string>
#include <vector>
#include <ncurses.h>
#include <deque>
#include <stack>

#define COLOR_DIR 1
#define COLOR_FILE 2
#define COLOR_LINK 3
#define COLOR_HEADER 4
#define COLOR_HIGHLIGHT 5

void initColors();
void printColored(WINDOW* win, const std::string& text, int colorPair, bool highlight = false);
std::string browseAndSelectFileImpl(const std::string& startDir, 
                                   std::deque<std::string>& dirHistory, 
                                   std::stack<std::string>& backStack);

#endif // UI_H
