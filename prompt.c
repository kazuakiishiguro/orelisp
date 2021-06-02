#include <stdio.h>
#include <stdlib.h>

/* for Windows */
#ifdef _WIN32

#include <string.h>

/* pesudo readline function */
char *readline(char *prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char *cpy = malloc(strlen(buffer)+1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy)-1] = '\0';
    return cpy;
}

/* pesudo add_history function */
void add_history(char * unused) {}

#else
#include <editline/readline.h>
#endif

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
