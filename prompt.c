#include <stdio.h>

static char input[2048];

int main(int argc, char **argv) {
    puts("Orelisp version 0.0.1");
    puts("Press Ctl+c to Exit\n");

    while (1) {
        fputs("orelisp> ", stdout);
        fgets(input, 2048, stdin);
        printf("[Echoing....] %s", input);
    }
    return 0;
}
