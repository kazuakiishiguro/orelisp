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

#define LASSERT(args, cond, err) \
    if (!(cond)) { lval_del(args); return lval_err(err); }

/* declare lval(lisp value) struct */
typedef struct lval{
    int type;
    long num;
    char *err;
    char *sym;
    int count;
    struct lval **cell;
} lval;

/* for lval types */
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

lval *lval_err(char *m) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
    return v;
}

lval *lval_num(long x) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

lval *lval_sym(char *s) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

lval *lval_sexpr(void) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval *lval_qexpr(void) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell =NULL;
    return v;
}

void lval_del(lval *v) {
    switch(v->type) {
        case LVAL_NUM: break;
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;

        case LVAL_SEXPR:
        case LVAL_QEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            free(v->cell);
        break;
    }

    free(v);
}

lval *lval_read_num(mpc_ast_t *t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ?
            lval_num(x) : lval_err("invalid number");
}

lval *lval_add(lval *v, lval *x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

lval *lval_read(mpc_ast_t *t) {
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    lval *x = NULL;
    /* if root (>) or sexpr then create empty list */
    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr")) { x = lval_sexpr(); }
    if (strstr(t->tag, "qexpr")) { x = lval_qexpr(); }

    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

void lval_print(lval* v);

void lval_expr_print(lval *v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        lval_print(v->cell[i]);
        if (i != (v->count-1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_print(lval *v) {
    switch (v->type) {
        case LVAL_NUM: printf("%li", v->num); break;
        case LVAL_ERR: printf("err: %s", v->err); break;
        case LVAL_SYM: printf("%s", v->sym); break;
        case LVAL_SEXPR: lval_expr_print(v, '(', ')');  break;
        case LVAL_QEXPR: lval_expr_print(v, '{', '}');  break;
    }
}

void lval_println(lval *v) {
    lval_print(v); putchar('\n');
}

lval *lval_eval_sexpr(lval *v);

lval *lval_eval(lval *v) {
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
    return v;
}

lval *lval_pop(lval *v, int i) {
    lval *x = v->cell[i];

    /* shift memory after the item at "i" over the top */
    memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));
    /* decrese the count */
    v->count--;
    /* reallocate memory */
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

lval *lval_take(lval *v, int i) {
    lval *x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval *lval_join(lval *x, lval *y) {
    while(y->count) {
        x = lval_add(x, lval_pop(y, 0));
    }

    return x;
}

lval *builtin_op(lval *v, char *op) {
    /* make sure all arguments are numbers */
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type != LVAL_NUM) {
            lval_del(v);
            return lval_err("Cannot operate on num-number!");
        }
    }

    lval *x = lval_pop(v, 0);

    if ((strcmp(op, "-") == 0) && v->count == 0) {
        x->num = -x->num;
    }

    while (v->count > 0) {
        lval * y = lval_pop(v, 0);
        if (strcmp(op, "+") == 0) { x->num += y->num; }
        if (strcmp(op, "-") == 0) { x->num -= y->num; }
        if (strcmp(op, "*") == 0) { x->num *= y->num; }
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                lval_del(x); lval_del(y);
                x = lval_err("Division by Zero!"); break;
            }
            x->num /= y->num;
        }
        lval_del(y);
    }
    lval_del(v);
    return x;
}

lval *builtin_head(lval *v) {
    LASSERT(v, v->count==1,
            "Function 'head' passed too many arguments");
    LASSERT(v, v->cell[0]->type == LVAL_QEXPR,
            "Function 'head' passed incorrect types");
    LASSERT(v, v->cell[0]->count != 0,
            "Function 'head' passed {}");

    lval *v_first = lval_take(v, 0);
    while(v_first->count > 1) {
        lval_del(lval_pop(v_first, 1));
    }
    return v_first;
}

lval *builtin_tail(lval *v) {
    LASSERT(v, v->count==1,
            "Function 'tail' passed too many arguments");
    LASSERT(v, v->cell[0]->type == LVAL_QEXPR,
            "Function 'tail' passed incorrect types");
    LASSERT(v, v->cell[0]->count != 0,
            "Function 'tail' passed {}");

    lval *v_tail = lval_take(v, 0);
    lval_del(lval_pop(v_tail, 0));
    return v_tail;
}

lval *builtin_list(lval *v) {
    v->type = LVAL_QEXPR;
    return v;
}

lval *builtin_eval(lval *v) {
    LASSERT(v, v->count == 1,
            "Function 'eval' passed too many arguments");
    LASSERT(v, v->cell[0]->type == LVAL_QEXPR,
            "Function 'eval' passed incorrect types");

    lval *v_eval = lval_take(v, 0);
    v_eval->type = LVAL_SEXPR;
    return lval_eval(v_eval);
}

lval *builtin_join(lval *v) {
    for (int i = 0; i < v->count; i++) {
        LASSERT(v, v->cell[i]->type == LVAL_QEXPR,
                "Function 'join' passed incorrect types");
    }

    lval *x = lval_pop(v, 0);

    while (v->count) {
        x = lval_join(x, lval_pop(v, 0));
    }

    lval_del(v);

    return x;
}

lval *builtin(lval *v, char *func) {
    if (strcmp("list", func) == 0) { return builtin_list(v); }
    if (strcmp("head", func) == 0) { return builtin_head(v); }
    if (strcmp("tail", func) == 0) { return builtin_tail(v); }
    if (strcmp("join", func) == 0) { return builtin_join(v); }
    if (strcmp("eval", func) == 0) { return builtin_eval(v); }
    if (strstr("+-/*", func)) { return builtin_op(v, func); }
    lval_del(v);
    return lval_err("Unknown Function");
}

lval *lval_eval_sexpr(lval *v) {
    /* evaluate children */
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    /* error check and return the first error */
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }

    if (v->count == 0) { return v; }

    if (v->count == 1) { return lval_take(v, 0); }

    /* make sure the first element is always symbol */
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f); lval_del(v);
        return lval_err("S-expression does not start with symbol!");
    }

    lval *result = builtin(v, f->sym);
    lval_del(f);

    return result;
}

int main(int argc, char **argv) {
    /* Create Some Parsers */
    mpc_parser_t* Number   = mpc_new("number");
    mpc_parser_t* Symbol   = mpc_new("symbol");
    mpc_parser_t* Sexpr    = mpc_new("sexpr");
    mpc_parser_t* Qexpr    = mpc_new("qexpr");
    mpc_parser_t* Expr     = mpc_new("expr");
    mpc_parser_t* Orelisp  = mpc_new("orelisp");

    /* Define them with the following Language */
    mpca_lang(MPCA_LANG_DEFAULT,
    "                                                           \
        number : /-?[0-9]+/ ;                                   \
        symbol : \"list\" | \"head\" | \"tail\"                 \
               | \"join\" | \"eval\" | '+' | '-' | '*' | '/' ;  \
        sexpr  : '(' <expr>* ')' ;                              \
        qexpr  : '{' <expr>* '}' ;                              \
        expr   : <number> | <symbol> | <sexpr> | <qexpr>;       \
        orelisp  : /^/ <expr>* /$/ ;                            \
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Orelisp);

    puts("Orelisp version 0.0.2");
    puts("Press Ctl+c to Exit\n");

    while (1) {
        char *input = readline("orelisp> ");
        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Orelisp, &r)) {
            lval *x = lval_eval(lval_read(r.output));
            lval_println(x);
            lval_del(x);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    /* undefine and delete our parser */
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Orelisp);

    return 0;
}
