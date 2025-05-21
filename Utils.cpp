#include "Utils.h"
#include <iostream>
#include <dirent.h> // Added for DT_DIR, DT_REG, DT_LNK, DT_UNKNOWN

using namespace std;

// Clears the terminal screen
void clearScreen() {
    cout << "\033[2J\033[1;1H";
}

// Returns the file type as a string based on the type code
string getFileType(char type) {
    switch(type) {
        case DT_DIR: return "d (directory)";
        case DT_REG: return "f (file)";
        case DT_LNK: return "l (symlink)";
        case DT_UNKNOWN: return "? (unknown)";
        default: return "? (special)";
    }
}
