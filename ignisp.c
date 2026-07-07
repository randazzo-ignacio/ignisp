/* ignisp 0.2 -- layered kernel
 *
 * THE KERNEL (Layer 1): This is the only host-dependent code.
 * Everything above this layer is implemented in ignisp itself.
 *
 * The kernel provides:
 *   - Tagged value representation (fixnum, symbol, cons, string, array, closure, primitive)
 *   - Bootstrap reader (S-expression parser, just enough to load stdlib.lisp)
 *   - Evaluator with 8 special forms
 *   - 21 primitives
 *   - File loading (loads stdlib.lisp at startup)
 *   - REPL (uses Layer 2 printer if available, C debug printer as fallback)
 *
 * Primitives (21):
 *   cons, car, cdr, eq, +, -, *, /, <, >, =,
 *   make-array, aref, aset, array-length, type-of, symbol-name,
 *   read-char, write-char, error, eval
 *
 * Special forms (8):
 *   if, quote, lambda, let, setq, defmacro, define, begin
 *
 * Build: gcc -o ignisp ignisp.c
 * Run:   ./ignisp
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>

typedef struct Obj Obj;

/* ---- Types ---- */
enum { T_FIXNUM, T_SYMBOL, T_CONS, T_STRING, T_ARRAY, T_CLOSURE, T_PRIMITIVE };

struct Obj {
    int type;
    union {
        long fixnum;
        char *symbol;
        struct { Obj *car, *cdr; } cons;
        char *string;
        struct { Obj **data; long size; } array;
        struct { Obj *params, *body, *env; } closure;
        Obj *(*prim)(Obj *args);
    };
};

/* ---- Globals ---- */
Obj *NIL, *T;
Obj *global_env;
Obj *macro_table;
Obj *symbol_list;
Obj *sym_if, *sym_quote, *sym_lambda, *sym_let, *sym_setq,
   *sym_defmacro, *sym_define, *sym_begin, *sym_prin1;

FILE *input = NULL;
jmp_buf error_jmp;

void error(const char *msg) {
    fprintf(stderr, "error: %s\n", msg);
    longjmp(error_jmp, 1);
}

/* ---- Constructors -- malloc everything, never free (Phase 1: no GC) ---- */

Obj *make_fixnum(long n) {
    Obj *o = malloc(sizeof(Obj));
    o->type = T_FIXNUM;
    o->fixnum = n;
    return o;
}

Obj *make_cons(Obj *car, Obj *cdr) {
    Obj *o = malloc(sizeof(Obj));
    o->type = T_CONS;
    o->cons.car = car;
    o->cons.cdr = cdr;
    return o;
}

Obj *make_string(const char *s) {
    Obj *o = malloc(sizeof(Obj));
    o->type = T_STRING;
    o->string = strdup(s);
    return o;
}

Obj *make_array(long size) {
    Obj *o = malloc(sizeof(Obj));
    o->type = T_ARRAY;
    o->array.size = size;
    o->array.data = malloc(sizeof(Obj*) * size);
    for (long i = 0; i < size; i++)
        o->array.data[i] = NIL;
    return o;
}

Obj *make_closure(Obj *params, Obj *body, Obj *env) {
    Obj *o = malloc(sizeof(Obj));
    o->type = T_CLOSURE;
    o->closure.params = params;
    o->closure.body = body;
    o->closure.env = env;
    return o;
}

Obj *make_primitive(Obj *(*fn)(Obj *args)) {
    Obj *o = malloc(sizeof(Obj));
    o->type = T_PRIMITIVE;
    o->prim = fn;
    return o;
}

/* ---- Symbol interning ---- */

Obj *intern(const char *name) {
    for (Obj *p = symbol_list; p != NIL; p = p->cons.cdr) {
        if (strcmp(p->cons.car->symbol, name) == 0)
            return p->cons.car;
    }
    Obj *s = malloc(sizeof(Obj));
    s->type = T_SYMBOL;
    s->symbol = strdup(name);
    symbol_list = make_cons(s, symbol_list);
    return s;
}

/* ---- Environment -- assoc list: ((sym . val) (sym . val) ...) ---- */

Obj *lookup(Obj *sym, Obj *env) {
    for (Obj *e = env; e != NIL; e = e->cons.cdr) {
        Obj *binding = e->cons.car;
        if (binding->cons.car == sym)
            return binding->cons.cdr;
    }
    /* Fallback: check current global env.
     * This allows forward references -- a closure captures global_env
     * at definition time, but later definitions are prepended to
     * global_env. The fallback finds them. */
    for (Obj *e = global_env; e != NIL; e = e->cons.cdr) {
        Obj *binding = e->cons.car;
        if (binding->cons.car == sym)
            return binding->cons.cdr;
    }
    fprintf(stderr, "error: unbound variable: %s\n", sym->symbol);
    longjmp(error_jmp, 1);
    return NIL;
}

Obj *bind(Obj *sym, Obj *val, Obj *env) {
    return make_cons(make_cons(sym, val), env);
}

Obj *set_var(Obj *sym, Obj *val, Obj *env) {
    for (Obj *e = env; e != NIL; e = e->cons.cdr) {
        Obj *binding = e->cons.car;
        if (binding->cons.car == sym) {
            binding->cons.cdr = val;
            return val;
        }
    }
    /* Not found -- create in global env */
    global_env = bind(sym, val, global_env);
    return val;
}

/* ---- Bootstrap reader ---- */
/* Just enough to parse S-expressions and load stdlib.lisp.
 * The "real" reader can be reimplemented in Layer 2 later. */

static int peek_char = -2;

static int peek() {
    if (peek_char == -2)
        peek_char = fgetc(input);
    return peek_char;
}

static int next_char() {
    if (peek_char != -2) {
        int c = peek_char;
        peek_char = -2;
        return c;
    }
    return fgetc(input);
}

static void skip_ws() {
    int c;
    while ((c = peek()) != EOF) {
        if (isspace(c)) {
            next_char();
        } else if (c == ';') {
            while ((c = next_char()) != EOF && c != '\n');
        } else {
            break;
        }
    }
}

static int is_number(const char *s) {
    if (*s == '-') s++;
    if (!*s) return 0;
    while (*s) {
        if (!isdigit(*s)) return 0;
        s++;
    }
    return 1;
}

Obj *read_form();

Obj *read_list() {
    skip_ws();
    int c = peek();
    if (c == ')') {
        next_char();
        return NIL;
    }
    if (c == EOF)
        error("unexpected EOF in list");
    /* Dotted pair: . followed by whitespace/delim */
    if (c == '.') {
        next_char();
        int c2 = peek();
        if (c2 == EOF || isspace(c2) || c2 == ')' || c2 == '(') {
            Obj *cdr = read_form();
            skip_ws();
            c = peek();
            if (c != ')') error("expected ) after dotted pair");
            next_char();
            return cdr;
        }
        /* Not a dot token -- symbol starting with . */
        char buf[256];
        int i = 0;
        buf[i++] = '.';
        while ((c = peek()) != EOF && !isspace(c) &&
               c != '(' && c != ')' && c != ';' && c != '"' && c != '\'') {
            if (i < 255) buf[i++] = next_char();
            else next_char();
        }
        buf[i] = '\0';
        if (is_number(buf))
            return make_fixnum(atol(buf));
        if (strcmp(buf, "nil") == 0) return NIL;
        if (strcmp(buf, "t") == 0) return T;
        return intern(buf);
    }
    Obj *first = read_form();
    Obj *rest = read_list();
    return make_cons(first, rest);
}

Obj *read_string_lit() {
    char buf[1024];
    int i = 0;
    int c;
    while ((c = next_char()) != EOF && c != '"') {
        if (c == '\\') {
            c = next_char();
            switch (c) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case '"': c = '"'; break;
                case '\\': c = '\\'; break;
                default: error("bad string escape");
            }
        }
        if (i < 1023) buf[i++] = c;
    }
    buf[i] = '\0';
    return make_string(buf);
}

Obj *read_atom() {
    char buf[256];
    int i = 0;
    int c;
    while ((c = peek()) != EOF && !isspace(c) &&
           c != '(' && c != ')' && c != ';' && c != '"' && c != '\'') {
        if (i < 255) buf[i++] = next_char();
        else next_char();
    }
    buf[i] = '\0';

    if (is_number(buf))
        return make_fixnum(atol(buf));
    if (strcmp(buf, "nil") == 0) return NIL;
    if (strcmp(buf, "t") == 0) return T;
    return intern(buf);
}

Obj *read_form() {
    skip_ws();
    int c = peek();
    if (c == EOF) return NULL;
    if (c == '(') {
        next_char();
        return read_list();
    }
    if (c == '\'') {
        next_char();
        Obj *form = read_form();
        if (!form) error("unexpected EOF after quote");
        return make_cons(sym_quote, make_cons(form, NIL));
    }
    if (c == '"') {
        next_char();
        return read_string_lit();
    }
    if (c == ')')
        error("unexpected )");
    return read_atom();
}

/* ---- C debug printer ---- */
/* Used as REPL fallback when Layer 2 printer is not available.
 * The "real" printer is implemented in stdlib.lisp. */

void print(Obj *obj) {
    if (obj == NIL) { printf("nil"); return; }
    if (obj == T) { printf("t"); return; }
    switch (obj->type) {
        case T_FIXNUM:
            printf("%ld", obj->fixnum);
            break;
        case T_SYMBOL:
            printf("%s", obj->symbol);
            break;
        case T_STRING:
            printf("\"%s\"", obj->string);
            break;
        case T_CONS:
            printf("(");
            print(obj->cons.car);
            {
                Obj *p = obj->cons.cdr;
                while (p != NIL && p->type == T_CONS) {
                    printf(" ");
                    print(p->cons.car);
                    p = p->cons.cdr;
                }
                if (p != NIL) {
                    printf(" . ");
                    print(p);
                }
            }
            printf(")");
            break;
        case T_ARRAY:
            printf("#<array:%ld>", obj->array.size);
            break;
        case T_CLOSURE:
            printf("#<closure>");
            break;
        case T_PRIMITIVE:
            printf("#<primitive>");
            break;
        default:
            printf("#<unknown>");
    }
}

/* ---- Evaluator ---- */

Obj *eval(Obj *form, Obj *env) {
    if (form == NIL) return NIL;
    if (form == T) return T;
    if (form->type == T_FIXNUM) return form;
    if (form->type == T_STRING) return form;
    if (form->type == T_ARRAY) return form;
    if (form->type == T_CLOSURE) return form;
    if (form->type == T_PRIMITIVE) return form;
    if (form->type == T_SYMBOL) return lookup(form, env);
    if (form->type != T_CONS) return form;

    Obj *op = form->cons.car;
    Obj *args = form->cons.cdr;

    /* Special forms */
    if (op->type == T_SYMBOL) {
        if (op == sym_quote)
            return args->cons.car;

        if (op == sym_if) {
            Obj *test = eval(args->cons.car, env);
            if (test != NIL)
                return eval(args->cons.cdr->cons.car, env);
            {
                Obj *else_part = args->cons.cdr->cons.cdr;
                if (else_part == NIL) return NIL;
                return eval(else_part->cons.car, env);
            }
        }

        if (op == sym_lambda)
            return make_closure(args->cons.car, args->cons.cdr, env);

        if (op == sym_let) {
            Obj *bindings = args->cons.car;
            Obj *body = args->cons.cdr;
            Obj *new_env = env;
            for (Obj *b = bindings; b != NIL; b = b->cons.cdr) {
                Obj *var = b->cons.car->cons.car;
                Obj *val = eval(b->cons.car->cons.cdr->cons.car, env);
                new_env = bind(var, val, new_env);
            }
            Obj *result = NIL;
            for (Obj *bd = body; bd != NIL; bd = bd->cons.cdr)
                result = eval(bd->cons.car, new_env);
            return result;
        }

        if (op == sym_setq) {
            Obj *var = args->cons.car;
            Obj *val = eval(args->cons.cdr->cons.car, env);
            return set_var(var, val, env);
        }

        if (op == sym_defmacro) {
            Obj *name, *params, *body;
            if (args->cons.car->type == T_CONS) {
                /* (defmacro (name . params) body...) */
                name = args->cons.car->cons.car;
                params = args->cons.car->cons.cdr;
                body = args->cons.cdr;
            } else {
                /* (defmacro name params body...) */
                name = args->cons.car;
                params = args->cons.cdr->cons.car;
                body = args->cons.cdr->cons.cdr;
            }
            macro_table = bind(name, make_closure(params, body, env), macro_table);
            return name;
        }

        if (op == sym_define) {
            Obj *target = args->cons.car;
            if (target->type == T_CONS) {
                /* (define (name params...) body...)
                   Pre-bind name so the closure can see itself (recursion). */
                global_env = bind(target->cons.car, NIL, global_env);
                Obj *cl = make_closure(target->cons.cdr, args->cons.cdr, global_env);
                global_env->cons.car->cons.cdr = cl;
                return target->cons.car;
            } else {
                /* (define name value) */
                global_env = bind(target, eval(args->cons.cdr->cons.car, env), global_env);
                return target;
            }
        }

        if (op == sym_begin) {
            Obj *result = NIL;
            for (Obj *a = args; a != NIL; a = a->cons.cdr)
                result = eval(a->cons.car, env);
            return result;
        }

        /* Macro expansion -- check macro_table before function lookup */
        for (Obj *m = macro_table; m != NIL; m = m->cons.cdr) {
            if (m->cons.car->cons.car == op) {
                Obj *mf = m->cons.car->cons.cdr;
                Obj *new_env = mf->closure.env;
                Obj *params = mf->closure.params;
                Obj *a = args;
                while (params != NIL && a != NIL) {
                    if (params->type == T_SYMBOL) {
                        /* rest param: bind to remaining args */
                        new_env = bind(params, a, new_env);
                        params = NIL;
                        a = NIL;
                        break;
                    }
                    new_env = bind(params->cons.car, a->cons.car, new_env);
                    params = params->cons.cdr;
                    a = a->cons.cdr;
                }
                /* Bind remaining rest param to nil if no more args */
                if (params != NIL && params->type == T_SYMBOL) {
                    new_env = bind(params, NIL, new_env);
                    params = NIL;
                }
                /* Bind any remaining individual params to nil */
                while (params != NIL) {
                    new_env = bind(params->cons.car, NIL, new_env);
                    params = params->cons.cdr;
                }
                Obj *result = NIL;
                for (Obj *b = mf->closure.body; b != NIL; b = b->cons.cdr)
                    result = eval(b->cons.car, new_env);
                return eval(result, env);
            }
        }
    }

    /* Function call */
    Obj *fn = eval(op, env);

    /* Evaluate arguments into a list */
    Obj *evaled = NIL;
    Obj *tail = NIL;
    for (Obj *a = args; a != NIL; a = a->cons.cdr) {
        Obj *cell = make_cons(eval(a->cons.car, env), NIL);
        if (evaled == NIL)
            evaled = tail = cell;
        else {
            tail->cons.cdr = cell;
            tail = cell;
        }
    }

    if (fn->type == T_PRIMITIVE)
        return fn->prim(evaled);

    if (fn->type == T_CLOSURE) {
        Obj *new_env = fn->closure.env;
        Obj *params = fn->closure.params;
        Obj *a = evaled;
        while (params != NIL && a != NIL) {
            if (params->type == T_SYMBOL) {
                /* rest param: bind to remaining args */
                new_env = bind(params, a, new_env);
                params = NIL;
                a = NIL;
                break;
            }
            new_env = bind(params->cons.car, a->cons.car, new_env);
            params = params->cons.cdr;
            a = a->cons.cdr;
        }
        /* Bind remaining rest param to nil if no more args */
        if (params != NIL && params->type == T_SYMBOL) {
            new_env = bind(params, NIL, new_env);
            params = NIL;
        }
        /* Bind any remaining individual params to nil */
        while (params != NIL) {
            new_env = bind(params->cons.car, NIL, new_env);
            params = params->cons.cdr;
        }
        Obj *result = NIL;
        for (Obj *b = fn->closure.body; b != NIL; b = b->cons.cdr)
            result = eval(b->cons.car, new_env);
        return result;
    }

    error("not callable");
    return NIL;
}

/* ---- Primitives ---- */

Obj *prim_car(Obj *a) {
    Obj *x = a->cons.car;
    if (x == NIL) return NIL;
    if (x->type != T_CONS) error("car: not a cons");
    return x->cons.car;
}

Obj *prim_cdr(Obj *a) {
    Obj *x = a->cons.car;
    if (x == NIL) return NIL;
    if (x->type != T_CONS) error("cdr: not a cons");
    return x->cons.cdr;
}

Obj *prim_cons(Obj *a) {
    return make_cons(a->cons.car, a->cons.cdr->cons.car);
}

Obj *prim_add(Obj *a) {
    long r = 0;
    for (; a != NIL; a = a->cons.cdr) r += a->cons.car->fixnum;
    return make_fixnum(r);
}

Obj *prim_sub(Obj *a) {
    if (a == NIL) return make_fixnum(0);
    long r = a->cons.car->fixnum;
    a = a->cons.cdr;
    if (a == NIL) return make_fixnum(-r);
    for (; a != NIL; a = a->cons.cdr) r -= a->cons.car->fixnum;
    return make_fixnum(r);
}

Obj *prim_mul(Obj *a) {
    long r = 1;
    for (; a != NIL; a = a->cons.cdr) r *= a->cons.car->fixnum;
    return make_fixnum(r);
}

Obj *prim_div(Obj *a) {
    if (a == NIL) error("/: no args");
    long r = a->cons.car->fixnum;
    for (a = a->cons.cdr; a != NIL; a = a->cons.cdr) {
        if (a->cons.car->fixnum == 0) error("division by zero");
        r /= a->cons.car->fixnum;
    }
    return make_fixnum(r);
}

Obj *prim_lt(Obj *a) {
    return (a->cons.car->fixnum < a->cons.cdr->cons.car->fixnum) ? T : NIL;
}

Obj *prim_gt(Obj *a) {
    return (a->cons.car->fixnum > a->cons.cdr->cons.car->fixnum) ? T : NIL;
}

Obj *prim_numeq(Obj *a) {
    return (a->cons.car->fixnum == a->cons.cdr->cons.car->fixnum) ? T : NIL;
}

Obj *prim_eq(Obj *a) {
    Obj *x = a->cons.car, *y = a->cons.cdr->cons.car;
    if (x == y) return T;
    if (x->type == T_FIXNUM && y->type == T_FIXNUM && x->fixnum == y->fixnum)
        return T;
    return NIL;
}

Obj *prim_make_array(Obj *a) {
    return make_array(a->cons.car->fixnum);
}

Obj *prim_aref(Obj *a) {
    Obj *arr = a->cons.car;
    long i = a->cons.cdr->cons.car->fixnum;
    if (arr->type == T_ARRAY) {
        if (i < 0 || i >= arr->array.size) error("aref: out of bounds");
        return arr->array.data[i];
    } else if (arr->type == T_STRING) {
        long len = (long)strlen(arr->string);
        if (i < 0 || i >= len) error("aref: out of bounds");
        return make_fixnum((unsigned char)arr->string[i]);
    } else {
        error("aref: not an array or string");
        return NIL;
    }
}

Obj *prim_aset(Obj *a) {
    Obj *arr = a->cons.car;
    long i = a->cons.cdr->cons.car->fixnum;
    Obj *val = a->cons.cdr->cons.cdr->cons.car;
    if (arr->type != T_ARRAY) error("aset: not an array");
    if (i < 0 || i >= arr->array.size) error("aset: out of bounds");
    arr->array.data[i] = val;
    return val;
}

Obj *prim_array_length(Obj *a) {
    Obj *arr = a->cons.car;
    if (arr->type == T_ARRAY) return make_fixnum(arr->array.size);
    if (arr->type == T_STRING) return make_fixnum((long)strlen(arr->string));
    error("array-length: not an array or string");
    return NIL;
}

Obj *prim_type_of(Obj *a) {
    Obj *obj = a->cons.car;
    switch (obj->type) {
        case T_FIXNUM:    return intern("fixnum");
        case T_SYMBOL:    return intern("symbol");
        case T_CONS:      return intern("cons");
        case T_STRING:    return intern("string");
        case T_ARRAY:     return intern("array");
        case T_CLOSURE:   return intern("closure");
        case T_PRIMITIVE: return intern("primitive");
        default:          return intern("unknown");
    }
}

Obj *prim_symbol_name(Obj *a) {
    Obj *sym = a->cons.car;
    if (sym->type != T_SYMBOL) error("symbol-name: not a symbol");
    return make_string(sym->symbol);
}

Obj *prim_read_char(Obj *a) {
    int c = fgetc(input);
    return (c == EOF) ? NIL : make_fixnum(c);
}

Obj *prim_write_char(Obj *a) {
    putchar(a->cons.car->fixnum);
    return a->cons.car;
}

Obj *prim_error(Obj *a) {
    fprintf(stderr, "error: ");
    print(a->cons.car);
    fprintf(stderr, "\n");
    longjmp(error_jmp, 1);
    return NIL;
}

Obj *prim_eval(Obj *a) {
    return eval(a->cons.car, global_env);
}

/* ---- Initialization ---- */

void def_prim(const char *name, Obj *(*fn)(Obj *args)) {
    global_env = bind(intern(name), make_primitive(fn), global_env);
}

void init() {
    NIL = malloc(sizeof(Obj));
    NIL->type = T_SYMBOL;
    NIL->symbol = strdup("nil");

    T = malloc(sizeof(Obj));
    T->type = T_SYMBOL;
    T->symbol = strdup("t");

    symbol_list = make_cons(NIL, make_cons(T, NIL));
    global_env = NIL;
    macro_table = NIL;

    sym_if       = intern("if");
    sym_quote    = intern("quote");
    sym_lambda   = intern("lambda");
    sym_let      = intern("let");
    sym_setq     = intern("setq");
    sym_defmacro = intern("defmacro");
    sym_define   = intern("define");
    sym_begin    = intern("begin");
    sym_prin1    = intern("prin1");

    /* 21 primitives -- the complete kernel function set */
    def_prim("car", prim_car);
    def_prim("cdr", prim_cdr);
    def_prim("cons", prim_cons);
    def_prim("+", prim_add);
    def_prim("-", prim_sub);
    def_prim("*", prim_mul);
    def_prim("/", prim_div);
    def_prim("<", prim_lt);
    def_prim(">", prim_gt);
    def_prim("=", prim_numeq);
    def_prim("eq", prim_eq);
    def_prim("make-array", prim_make_array);
    def_prim("aref", prim_aref);
    def_prim("aset", prim_aset);
    def_prim("array-length", prim_array_length);
    def_prim("type-of", prim_type_of);
    def_prim("symbol-name", prim_symbol_name);
    def_prim("read-char", prim_read_char);
    def_prim("write-char", prim_write_char);
    def_prim("error", prim_error);
    def_prim("eval", prim_eval);
}

/* ---- File loading ---- */

void load_file(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "warning: cannot load %s\n", filename);
        return;
    }
    FILE *prev_input = input;
    int prev_peek = peek_char;
    input = f;
    peek_char = -2;

    if (setjmp(error_jmp)) {
        fprintf(stderr, "error loading %s\n", filename);
    } else {
        while (1) {
            Obj *form = read_form();
            if (!form) break;
            eval(form, global_env);
        }
    }

    fclose(f);
    input = prev_input;
    peek_char = prev_peek;
}

/* ---- REPL ---- */

int main() {
    init();
    input = stdin;

    /* Load standard library (Layer 2) */
    load_file("stdlib.lisp");

    printf("ignisp 0.2\n");
    while (1) {
        if (setjmp(error_jmp)) {
            peek_char = -2;
            int c;
            while ((c = fgetc(input)) != EOF && c != '\n');
        }
        printf("> ");
        fflush(stdout);
        Obj *form = read_form();
        if (!form) {
            printf("\n");
            break;
        }
        Obj *result = eval(form, global_env);

        /* Try Layer 2 printer (prin1), fall back to C debug printer */
        Obj *prin1_fn = NULL;
        for (Obj *e = global_env; e != NIL; e = e->cons.cdr) {
            if (e->cons.car->cons.car == sym_prin1) {
                prin1_fn = e->cons.car->cons.cdr;
                break;
            }
        }
        if (prin1_fn && (prin1_fn->type == T_CLOSURE ||
                         prin1_fn->type == T_PRIMITIVE)) {
            /* Call (prin1 (quote result)) -- quote ensures the value
               is passed as-is, not re-evaluated */
            Obj *call = make_cons(sym_prin1,
                make_cons(make_cons(sym_quote, make_cons(result, NIL)), NIL));
            eval(call, global_env);
        } else {
            print(result);
        }
        printf("\n");
    }
    return 0;
}