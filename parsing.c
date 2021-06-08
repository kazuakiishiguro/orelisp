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

/* forward declarations */
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/* list value */
enum {
    LVAL_NUM,   // number
    LVAL_ERR,   // error
    LVAL_SYM,   // symbol
    LVAL_FUN,   // function
    LVAL_SEXPR, // s-expression
    LVAL_QEXPR  // q-expression
};

typedef lval *(*lbuiltin)(lenv *, lval *);

/* declare lval(lisp value) struct */
typedef struct lval{
    int type;
    long num;
    char *err;
    char *sym;
    lbuiltin fun;
    int count;
    struct lval **cell;
} lval;

lval *lval_err(char *fmt, ...) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;

    va_list va;
    va_start(va, fmt);

    v->err = malloc(512);
    vsnprintf(v->err, 511, fmt, va);

    v->err = realloc(v->err, strlen(v->err) + 1);
    va_end(va);

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

lval *lval_fun(lbuiltin func) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->fun = func;
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
        case LVAL_NUM:
        case LVAL_FUN: break;
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
        case LVAL_FUN: printf("<function>"); break;
        case LVAL_SEXPR: lval_expr_print(v, '(', ')');  break;
        case LVAL_QEXPR: lval_expr_print(v, '{', '}');  break;
    }
}

void lval_println(lval *v) {
    lval_print(v); putchar('\n');
}

char *ltype_name(int t) {
    switch(t) {
        case LVAL_FUN: return "Function";
        case LVAL_NUM: return "Number";
        case LVAL_ERR: return "Error";
        case LVAL_SYM: return "Symbol";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
}

lval *lval_copy(lval *v) {
    lval *x = malloc(sizeof(lval));
    x->type = v->type;

    switch (v->type) {
        case LVAL_FUN: x->fun = v->fun; break;
        case LVAL_NUM: x->num = v->num; break;
        case LVAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err); break;
        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym); break;

        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval *) * x->count);
            for (int i = 0; i < x->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
            break;
    }
    return x;
}

lval *lval_eval_sexpr(lenv *e, lval *v);
lval *lenv_get(lenv *e, lval *v);

lval *lval_eval(lenv *e,lval *v) {
    if (v->type == LVAL_SYM) {
        lval *x = lenv_get(e, v);
        lval_del(v);
        return x;
    }

    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
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

/*
 * lisp environment
 */
struct lenv {
    int count;
    char **syms;
    lval **vals;
};

lenv *lenv_new(void) {
    lenv *e = malloc(sizeof(lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void lenv_del(lenv *e) {
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

lval *lenv_get(lenv *e, lval *v) {
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], v->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }

    return lval_err("unbound symbol '%s'", v->sym);
}

void lenv_put(lenv *e, lval *k, lval *v) {
    for (int i = 0; i < e->count; i++) {
        if(strcmp(e->syms[i], k->sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    e->count++;
    e->vals = realloc(e->vals, sizeof(lval *) * e->count);
    e->syms = realloc(e->syms, sizeof(char *) * e->count);

    e->vals[e->count-1] = lval_copy(v);
    e->syms[e->count-1] = malloc(strlen(k->sym)+1);
    strcpy(e->syms[e->count-1], k->sym);
}

/*
 * Builtins
 */

#define LASSERT(args, cond, fmt, ...)                           \
    if (!(cond)) {                                              \
        lval *err = lval_err(fmt, ##__VA_ARGS__);               \
        lval_del(args);                                         \
        return err;                                             \
    }                                                           \

#define LASSERT_TYPE(func, args, index, expect)                         \
    LASSERT(args, args->cell[index]->type == expect,                    \
            "Function '%s' passed incorrect type for argument %i. Got %s, Expected %s", \
            func, index, ltype_name(args->cell[index]->type), ltype_name(expect)) \

#define LASSERT_NUM(func, args, num)                                    \
    LASSERT(args, args->count == num,                                   \
            "Function '%s' passed incorrect number of arguments. Got %i, Expected %i", \
            func, args->count, num)                                     \

#define LASSERT_NOT_EMPTY(func, args, index)                            \
    LASSERT(args, args->cell[index]->count != 0,                        \
            "Function '%s' passed {} for argument %i.", func, index);   \


lval *builtin_op(lenv *e, lval *v, char *op) {
    /* make sure all arguments are numbers */
    for (int i = 0; i < v->count; i++) {
        LASSERT_TYPE(op, v, i, LVAL_NUM)
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

lval *builtin_add(lenv *e, lval *v) {
    return builtin_op(e, v, "+");
}

lval *builtin_sub(lenv *e, lval *v) {
    return builtin_op(e, v, "-");
}

lval *builtin_mul(lenv *e, lval *v) {
    return builtin_op(e, v, "*");
}

lval *builtin_div(lenv *e, lval *v) {
    return builtin_op(e, v, "/");
}

lval *builtin_head(lenv *e, lval *v) {
    LASSERT_NUM("head", v, 1);
    LASSERT_TYPE("head", v, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("head", v, 0);

    lval *v_first = lval_take(v, 0);
    while(v_first->count > 1) {
        lval_del(lval_pop(v_first, 1));
    }
    return v_first;
}

lval *builtin_tail(lenv *e, lval *v) {
    LASSERT_NUM("tail", v, 1);
    LASSERT_TYPE("tail", v, 0, LVAL_QEXPR);

    lval *v_tail = lval_take(v, 0);
    lval_del(lval_pop(v_tail, 0));
    return v_tail;
}

lval *builtin_list(lenv *e, lval *v) {
    v->type = LVAL_QEXPR;
    return v;
}

lval *builtin_eval(lenv *e, lval *v) {
    LASSERT_NUM("eval", v, 1);
    LASSERT_TYPE("eval", v, 0, LVAL_QEXPR);

    lval *v_eval = lval_take(v, 0);
    v_eval->type = LVAL_SEXPR;
    return lval_eval(e, v_eval);
}

lval *builtin_join(lenv *e, lval *v) {
    for (int i = 0; i < v->count; i++) {
        LASSERT_TYPE("join", v, i, LVAL_QEXPR);
    }

    lval *x = lval_pop(v, 0);

    while (v->count) {
        x = lval_join(x, lval_pop(v, 0));
    }

    lval_del(v);

    return x;
}

lval *builtin_def(lenv *e, lval *v) {
    LASSERT_TYPE("def", v, 0, LVAL_QEXPR);

    lval *syms = v->cell[0];

    for (int i = 0; i < syms->count; i++) {
        LASSERT(v, (syms->cell[i]->type == LVAL_SYM),
                "Function 'def' cannot define non-symbol"
                "Got %s, Expected %s",
                ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
    }

    LASSERT(v, (syms->count == v->count-1),
            "Function 'def' passed too many arguments for symbols "
            "Got %i, Expected %i",
            syms->count, v->count-1);

    for (int i = 0; i < syms->count; i++) {
        lenv_put(e, syms->cell[i], v->cell[i+1]);
    }

    lval_del(v);
    return lval_sexpr();
}

void lenv_add_builtin(lenv *e, char *name, lbuiltin func) {
    lval *k = lval_sym(name);
    lval *v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k);
    lval_del(v);
}

void lenv_add_builtins(lenv *e) {
    /* variable functions */
    lenv_add_builtin(e, "def", builtin_def);

    /* list functions */
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);

    /* mathmatical functions */
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);
}

lval *lval_eval_sexpr(lenv *e, lval *v) {
    /* evaluate children */
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    /* error check and return the first error */
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }

    if (v->count == 0) { return v; }
    if (v->count == 1) { return lval_take(v, 0); }

    /* make sure the first element is always symbol */
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
        lval_del(v);
        lval_del(f);
        return lval_err("first element is not a function");
    }

    /* if so, call function to get result */
    lval *result = f->fun(e, v);
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
        symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;             \
        sexpr  : '(' <expr>* ')' ;                              \
        qexpr  : '{' <expr>* '}' ;                              \
        expr   : <number> | <symbol> | <sexpr> | <qexpr>;       \
        orelisp  : /^/ <expr>* /$/ ;                            \
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Orelisp);

    puts("Orelisp version 0.0.3");
    puts("Press Ctl+c to Exit\n");

    lenv *e = lenv_new();
    lenv_add_builtins(e);

    while (1) {
        char *input = readline("orelisp> ");
        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Orelisp, &r)) {
            lval *x = lval_eval(e, lval_read(r.output));
            lval_println(x);
            lval_del(x);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    lenv_del(e);

    /* undefine and delete our parser */
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Orelisp);

    return 0;
}
