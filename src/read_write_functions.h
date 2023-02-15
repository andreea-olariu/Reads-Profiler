#pragma once

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int read_string_from_fd(int fd, char* storage, const char err_msg[]) {
    int cod_term, index = 0, nr_bytes;
    char ch;
    read(fd, &nr_bytes, sizeof(int));
    while (nr_bytes--)
    {
        cod_term = read(fd, &ch, 1);
        if(cod_term <= 0) {
            break;
        }
        storage[index++] = ch;
    }
}
void write_string_to_fd(int fd, char to_write[], char err_msg[]) {
    int len = strlen(to_write);
    if(write(fd, &len, sizeof(int)) != 4) {
        perror(err_msg);
        exit(0);
    }
    if(len != write(fd, to_write, len)) {
        perror(err_msg);
        exit(0);
    }
}
