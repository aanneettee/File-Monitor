#ifndef NAVIGATION_H
#define NAVIGATION_H

#include <string>
#include <deque>
#include <ncurses.h>

void showHistory(const std::deque<std::string>& dirHistory);

#endif // NAVIGATION_H
