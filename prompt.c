#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>

static char input[2048];

int main(int argc, char **argv) {
    puts("Orelisp version 0.0.1");
    puts("Press Ctl+c to Exit\n");

    while (1) {
        char* input = readline("orelisp> ");
        add_history(input);
        printf("[Echoing....] %s\n", input);
        free(input);
    }
    return 0;
}
