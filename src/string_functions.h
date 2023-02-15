#pragma once

#include <string.h>

void strlower(char* book) {
    for (unsigned i = 0; book[i] != '\0'; i++) {
        if (book[i] >= 'A' && book[i] <= 'Z')
            book[i] = (char)(book[i] - 'A' + 'a');
    }
}

void strtrim(char* string) {
    while (string[0] == ' ' || string[0] == '\n')
        strcpy(string, string + 1);
    while (string[strlen(string) - 1] == ' ' || string[strlen(string) - 1] == '\n')
        string[strlen(string) - 1] = '\0';
}