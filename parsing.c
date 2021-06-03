#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

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

/* evaluate which operator should use */
long eval_op(long x, char * op, long y) {
    if (strcmp(op, "+") == 0) { return x + y; }
    if (strcmp(op, "-") == 0) { return x - y; }
    if (strcmp(op, "*") == 0) { return x * y; }
    if (strcmp(op, "/") == 0) { return x / y; }
    return 0;
}

long eval(mpc_ast_t *t) {
    if (strstr(t->tag, "number")) {
        return atoi(t->contents);
    }

    /* operator is always second child */
    char *op = t->children[1]->contents;

    /* store the third child in x */
    long x = eval(t->children[2]);

    /* iterate remaining children */
    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    return x;
}

int main(int argc, char **argv) {
    /* Create Some Parsers */
    mpc_parser_t* Number   = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr     = mpc_new("expr");
    mpc_parser_t* Orelisp    = mpc_new("orelisp");

    /* Define them with the following Language */
    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                     \
        number   : /-?[0-9]+/ ;                             \
        operator : '+' | '-' | '*' | '/' ;                  \
        expr     : <number> | '(' <operator> <expr>+ ')' ;  \
        orelisp    : /^/ <operator> <expr>+ /$/ ;             \
        ",
        Number, Operator, Expr, Orelisp);

    puts("Orelisp version 0.0.2");
    puts("Press Ctl+c to Exit\n");

    while (1) {
        char *input = readline("orelisp> ");
        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Orelisp, &r)) {
            long result = eval(r.output);
            printf("%li\n", result);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    /* undefine and delete our parser */
    mpc_cleanup(4, Number, Operator, Expr, Orelisp);

    return 0;
}
