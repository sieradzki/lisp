#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mpc.h"

#ifdef _WIN32
#include <string.h>

static char buffer[2048];

char *readline(char *prompt)
{
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char *cpy = malloc(strlen(buffer) + 1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy) - 1] = '\n';
  return cpy;
}

void add_history(char *unused) {}

#else
#include <editline.h>
#endif

#define LASSERT(args, cond, fmt, ...)         \
  if (!(cond))                                \
  {                                           \
    lval *err = lval_err(fmt, ##__VA_ARGS__); \
    lval_del(args);                           \
    return err;                               \
  }


/* Assert definitions */

#define LASSERT_TYPE(args, func, index, required)                                      \
  LASSERT(args, args->cell[index]->type == required,                                   \
          "Function '%s' passed incorrect type for argument %i. Got %s, Expected %s.", \
          func, index, ltype_name(args->cell[index]->type), ltype_name(required));

#define LASSERT_COUNT(args, func, required)                                          \
  LASSERT(args, args->count == required,                                             \
          "Function '%s' passed incorrect number of arguments. Got %i, Expected %i", \
          func, args->count, required);

#define LASSERT_EMPTY(args, func, index)       \
  LASSERT(args, args->cell[index]->count != 0, \
          "Function '%s' passed {}!");

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/* Create Enumeration of Possible lval Types */
enum
{
  LVAL_NUM,
  LVAL_ERR,
  LVAL_SYM,
  LVAL_FUN,
  LVAL_SEXPR,
  LVAL_QEXPR
};

typedef lval *(*lbuiltin)(lenv *, lval *);

/* Declare New lval Struct */
struct lval
{
  int type;
  double num;

  char *err;
  char *sym;
  lbuiltin fun;

  int count;
  struct lval **cell;
};

/* Declare New lenv Struct */
struct lenv
{
  int count;
  char **syms;
  lval **vals;
};

/* Create lenv structure */
lenv *lenv_new(void)
{
  lenv *e = malloc(sizeof(lenv));
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

void lval_del(lval *v);

/* Delete lenv structure */
void lenv_del(lenv *e)
{
  for (int i = 0; i < e->count; i++)
  {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }

  free(e->syms);
  free(e->vals);
  free(e);
}

char *ltype_name(int t)
{
  switch (t)
  {
  case LVAL_FUN:
    return "Function";
  case LVAL_NUM:
    return "Number";
  case LVAL_ERR:
    return "Error";
  case LVAL_SYM:
    return "Symbol";
  case LVAL_SEXPR:
    return "S-Expression";
  case LVAL_QEXPR:
    return "Q-Expression";
  default:
    return "Unkown";
  }
}

lval *lval_err(char *fmt, ...);
lval *lval_copy(lval *v);

/* Check if variable already exists */
lval *lenv_get(lenv *e, lval *k)
{
  /* Iterate over all items in environment */
  for (int i = 0; i < e->count; i++)
  {
    /* Check if the stored string matches the symbol string */
    /* If it does, return a copy of the value */
    if (strcmp(e->syms[i], k->sym) == 0)
    {
      return lval_copy(e->vals[i]);
    }
  }
  /* If no symbol found return error */
  return lval_err("Unbound Symbol '%s'", k->sym);
}

/* Add new variable to the environment */
void lenv_put(lenv *e, lval *k, lval *v)
{
  /* Iterate over all items in environment */
  /* This is to see if variable already exits */
  for (int i = 0; i < e->count; i++)
  {
    /* If variable is found delete item at that position */
    /* And replace with variable supplied by user */
    if (strcmp(e->syms[i], k->sym) == 0)
    {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
      return;
    }
  }

  /* If no existing entry found allocate space for new entry */
  e->count++;
  e->vals = realloc(e->vals, sizeof(lval *) * e->count);
  e->syms = realloc(e->syms, sizeof(char *) * e->count);

  /* Copy contents of lval and symbol string into new location */
  e->vals[e->count - 1] = lval_copy(v);
  e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
  strcpy(e->syms[e->count - 1], k->sym);
}

/* Create a pointer to a new number lval */
lval *lval_num(double x)
{
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

/* Create a pointer to a new error lval */
lval *lval_err(char *fmt, ...)
{
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_ERR;

  /* Create a va list and initialize it */
  va_list va;
  va_start(va, fmt);

  /* Allocate 512 bytes of space */
  v->err = malloc(512);

  /* printf the error string with a maximum of 511 characters */
  vsnprintf(v->err, 511, fmt, va);

  /* Reallocate to number of bytes actually used */
  v->err = realloc(v->err, strlen(v->err) + 1);

  /* Cleanup our va list */
  va_end(va);

  return v;
}

/* Create a pointer to a new symbol lval */
lval *lval_sym(char *s)
{
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

/* A pointer to a new empty sexpr lval */
lval *lval_sexpr(void)
{
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

/* A pointer to a new empty Qexpr lval */
lval *lval_qexpr(void)
{
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

/* A pointer to a new empty lval_fun type */
lval *lval_fun(lbuiltin func)
{
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->fun = func;
  return v;
}

/* Destructor for lval types */
void lval_del(lval *v)
{
  switch (v->type)
  {
  case LVAL_NUM:
    break;

  case LVAL_ERR:
    free(v->err);
    break;
  case LVAL_SYM:
    free(v->sym);
    break;

  case LVAL_QEXPR:
  case LVAL_SEXPR:
    for (int i = 0; i < v->count; i++)
    {
      lval_del(v->cell[i]);
    }
    free(v->cell);
    break;

  case LVAL_FUN:
    break;
  }

  free(v);
}

lval *lval_copy(lval *v)
{
  lval *x = malloc(sizeof(lval));
  x->type = v->type;

  switch (v->type)
  {
  /* Copy Functions and Numbers Directly */
  case LVAL_FUN:
    x->fun = v->fun;
    break;
  case LVAL_NUM:
    x->num = v->num;
    break;

  /* Copy Strings using malloc and strcpy */
  case LVAL_ERR:
    x->err = malloc(strlen(v->err) + 1);
    strcpy(x->err, v->err);
    break;

  case LVAL_SYM:
    x->sym = malloc(strlen(v->sym) + 1);
    strcpy(x->sym, v->sym);
    break;

  /* Copy Lists by copying each sub-expression */
  case LVAL_SEXPR:
  case LVAL_QEXPR:
    x->count = v->count;
    x->cell = malloc(sizeof(lval *) * x->count);
    for (int i = 0; i < x->count; i++)
    {
      x->cell[i] = lval_copy(v->cell[i]);
    }
    break;
  }

  return x;
}

/* Read lval number */
lval *lval_read_num(mpc_ast_t *t)
{
  errno = 0;
  double x = strtof(t->contents, NULL);
  return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval *lval_add(lval *v, lval *x)
{
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval *) * v->count);
  v->cell[v->count - 1] = x;
  return v;
}

lval *lval_read(mpc_ast_t *t)
{
  /* If Symbol or Number return conversion to that type */
  if (strstr(t->tag, "number"))
  {
    return lval_read_num(t);
  }
  if (strstr(t->tag, "symbol"))
  {
    return lval_sym(t->contents);
  }

  /* If root (>) or sexpr then create empty list */
  lval *x = NULL;
  if (strcmp(t->tag, ">") == 0)
  {
    x = lval_sexpr();
  }
  if (strstr(t->tag, "sexpr"))
  {
    x = lval_sexpr();
  }
  if (strstr(t->tag, "qexpr"))
  {
    x = lval_qexpr();
  }

  /* Fill this list with any valid expression contained within */
  for (int i = 0; i < t->children_num; i++)
  {
    if (strcmp(t->children[i]->contents, "(") == 0)
    {
      continue;
    }
    if (strcmp(t->children[i]->contents, ")") == 0)
    {
      continue;
    }
    if (strcmp(t->children[i]->contents, "}") == 0)
    {
      continue;
    }
    if (strcmp(t->children[i]->contents, "{") == 0)
    {
      continue;
    }
    if (strcmp(t->children[i]->tag, "regex") == 0)
    {
      continue;
    }
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

/* Forward declaration of lval_print function */
void lval_print(lval *v);

/* Print S-Expression */
void lval_expr_print(lval *v, char open, char close)
{
  putchar(open);
  for (int i = 0; i < v->count; i++)
  {
    lval_print(v->cell[i]);
    if (i != (v->count - 1))
    {
      putchar(' ');
    }
  }
  putchar(close);
}

/* Print an "lval" */
void lval_print(lval *v)
{
  switch (v->type)
  {
  case LVAL_NUM:
    printf("%f", v->num);
    break;
  case LVAL_ERR:
    printf("Error: %s", v->err);
    break;
  case LVAL_SYM:
    printf("%s", v->sym);
    break;
  case LVAL_SEXPR:
    lval_expr_print(v, '(', ')');
    break;
  case LVAL_QEXPR:
    lval_expr_print(v, '{', '}');
    break;
  case LVAL_FUN:
    printf("<function>");
    break;
  }
}

/* Print an "lval" followed by a newline */
void lval_println(lval *v)
{
  lval_print(v);
  putchar('\n');
}

lval *lval_pop(lval *v, int i)
{
  /* Find the item at "i" */
  lval *x = v->cell[i];

  /* Shift memory after the item at "i" over the top */
  memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval *) * (v->count - i - 1));

  /* Decrease the ccount of items in the list */
  v->count--;

  /* Reallocate the memory used */
  v->cell = realloc(v->cell, sizeof(lval *) * v->count);

  return x;
}

lval *lval_take(lval *v, int i)
{
  lval *x = lval_pop(v, i);
  lval_del(v);
  return x;
}

lval *builtin_head(lenv *e, lval *a)
{
  /* Check Error Conditions */
  LASSERT_COUNT(a, "head", 1);
  LASSERT_TYPE(a, "head", 0, LVAL_QEXPR);
  LASSERT_EMPTY(a, "head", 0);

  /* Otherwise take first argument */
  lval *v = lval_take(a, 0);

  /* Delete all elements that are not head and return */
  while (v->count > 1)
  {
    lval_del(lval_pop(v, 1));
  }
  return v;
}

/* Takes a value and a Q-Expression and appends it to the front */
lval *builtin_cons(lenv *e, double x, lval *v)
{
  /* Check Error Conditions */
  LASSERT_TYPE(v, "cons", 0, LVAL_QEXPR);

  /* I am not sure if it is exactly what the author meant but maybe? */
  v = lval_add(v, lval_num(x));
  return v;
}

/* Return number of elements in a Q-Expression */
int builtin_len(lenv *e, lval *v)
{
  return v->count;
}

/* Return all of a Q-Expression except the final element */
lval *builtin_init(lenv *e, lval *v)
{
  /* Check Error Conditions */
  LASSERT_TYPE(v, "init", 0, LVAL_QEXPR);
  LASSERT_EMPTY(v, "init", 0);
  
  lval_pop(v, v->count - 1);

  return v;
}

/* Forward declaration of lval_eval */
lval *lval_eval(lenv *e, lval *v);

lval *builtin_tail(lenv *e, lval *a)
{
  /* Check Error Conditions */
  LASSERT_COUNT(a, "tail", 1);
  LASSERT_TYPE (a, "tail", 0, LVAL_QEXPR);
  LASSERT_EMPTY(a, "tail", 0);

  /* Take first argument */
  lval *v = lval_take(a, 0);

  /* Delete first element and return */
  lval_del(lval_pop(v, 0));
  return v;
}

lval *builtin_list(lenv *e, lval *a)
{
  a->type = LVAL_QEXPR;
  return a;
}

lval *builtin_eval(lenv *e, lval *a)
{
  LASSERT_COUNT(a, "eval", 1);
  LASSERT_TYPE (a, "eval", 0, LVAL_QEXPR);

  lval *x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}

lval *lval_join(lval *x, lval *y)
{
  /* For each cell in 'y' add it to 'x' */
  while (y->count)
  {
    x = lval_add(x, lval_pop(y, 0));
  }

  /* Delete the empty 'y' and return 'x' */
  lval_del(y);
  return x;
}

lval *builtin_join(lenv *e, lval *a)
{
  for (int i = 0; i < a->count; i++)
  {
    LASSERT_TYPE (a, "join", 0, LVAL_QEXPR);
  }

  lval *x = lval_pop(a, 0);

  while (a->count)
  {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

lval *builtin_op(lenv *e, lval *a, char *op)
{
  /* Ensure all arguments are numbers */
  for (int i = 0; i < a->count; i++)
  {
    LASSERT_TYPE(a, op, i, LVAL_NUM);
  }

  /* Pop the first element */
  lval *x = lval_pop(a, 0);

  /* If no arguments and sub the perform unary negation */
  if ((strcmp(op, "-") == 0) && a->count == 0)
  {
    x->num = -x->num;
  }

  /* While there are still elements remaining */
  while (a->count > 0)
  {
    /* Pop the next element */
    lval *y = lval_pop(a, 0);

    if (strcmp(op, "+") == 0)
    {
      x->num += y->num;
    }
    if (strcmp(op, "-") == 0)
    {
      x->num -= y->num;
    }
    if (strcmp(op, "*") == 0)
    {
      x->num *= y->num;
    }
    if (strcmp(op, "/") == 0)
    {
      if (y->num == 0)
      {
        lval_del(x);
        lval_del(y);
        x = lval_err("Division By Zero!");
        break;
      }
      x->num /= y->num;
    }
    if (strcmp(op, "%") == 0)
    {
      x->num = fmod(x->num, y->num);
    }
    if (strcmp(op, "add") == 0)
    {
      x->num += y->num;
    }
    if (strcmp(op, "sub") == 0)
    {
      x->num -= y->num;
    }
    if (strcmp(op, "mult") == 0)
    {
      x->num *= y->num;
    }
    if (strcmp(op, "div") == 0)
    {
      if (y->num == 0)
      {
        lval_del(x);
        lval_del(y);
        x = lval_err("Division By Zero!");
        break;
      }
      x->num /= y->num;
    }
    if (strcmp(op, "mod") == 0)
    {
      x->num = fmod(x->num, y->num);
    }
    if (strcmp(op, "max") == 0)
    {
      x->num = x->num > y->num ? x->num : y->num;
    }
    if (strcmp(op, "min") == 0)
    {
      x->num = x->num < y->num ? x->num : y->num;
    }
    if (strcmp(op, "pow") == 0)
    {
      x->num = pow(x->num, y->num);
    }
    lval_del(y);
  }

  lval_del(a);
  return x;
}

lval *builtin_add(lenv *e, lval *a)
{
  return builtin_op(e, a, "+");
}

lval *builtin_sub(lenv *e, lval *a)
{
  return builtin_op(e, a, "-");
}

lval *builtin_mul(lenv *e, lval *a)
{
  return builtin_op(e, a, "*");
}

lval *builtin_div(lenv *e, lval *a)
{
  return builtin_op(e, a, "/");
}

lval *builtin(lenv *e, lval *a, char *func)
{
  if (strcmp("list", func) == 0)
  {
    return builtin_list(e, a);
  }
  if (strcmp("head", func) == 0)
  {
    return builtin_head(e, a);
  }
  if (strcmp("tail", func) == 0)
  {
    return builtin_tail(e, a);
  }
  if (strcmp("join", func) == 0)
  {
    return builtin_join(e, a);
  }
  if (strcmp("eval", func) == 0)
  {
    return builtin_eval(e, a);
  }
  if (strstr("+-/*%", func))
  {
    return builtin_op(e, a, func);
  }
  if (strcmp("add", func) == 0)
  {
    return builtin_op(e, a, func);
  }
  if (strcmp("sub", func) == 0)
  {
    return builtin_op(e, a, func);
  }
  if (strcmp("mult", func) == 0)
  {
    return builtin_op(e, a, func);
  }
  if (strcmp("div", func) == 0)
  {
    return builtin_op(e, a, func);
  }
  if (strcmp("mod", func) == 0)
  {
    return builtin_op(e, a, func);
  }
  if (strcmp("pow", func) == 0)
  {
    return builtin_op(e, a, func);
  }
  if (strcmp("min", func) == 0)
  {
    return builtin_op(e, a, func);
  }
  if (strcmp("max", func) == 0)
  {
    return builtin_op(e, a, func);
  }
  lval_del(a);
  return lval_err("Unkown Function!");
}

lval *lval_eval_sexpr(lenv *e, lval *v)
{
  /* Evaluate Children */
  for (int i = 0; i < v->count; i++)
  {
    v->cell[i] = lval_eval(e, v->cell[i]);
  }

  /* Error Checking */
  for (int i = 0; i < v->count; i++)
  {
    if (v->cell[i]->type == LVAL_ERR)
    {
      return lval_take(v, i);
    }
  }

  /* Empty Expression */
  if (v->count == 0)
  {
    return v;
  }

  /* Single Expression*/
  if (v->count == 1)
  {
    return lval_take(v, 0);
  }

  /* Ensure First Element is a function after evaluation */
  lval *f = lval_pop(v, 0);
  if (f->type != LVAL_FUN)
  {
    lval_del(f);
    lval_del(v);
    return lval_err("first element is not a function");
  }

  /* If so call function to get result */
  lval *result = f->fun(e, v);
  lval_del(f);
  return result;
}

lval *lval_eval(lenv *e, lval *v)
{
  if (v->type == LVAL_SYM)
  {
    lval *x = lenv_get(e, v);
    lval_del(v);
    return x;
  }

  if (v->type == LVAL_SEXPR)
  {
    return lval_eval_sexpr(e, v);
  }

  return v;
}

/* Register builtins into environment */
void lenv_add_builtin(lenv *e, char *name, lbuiltin func)
{
  lval *k = lval_sym(name);
  lval *v = lval_fun(func);
  lenv_put(e, k, v);
  lval_del(k);
  lval_del(v);
}

/* Define variable */
lval *builtin_def(lenv *e, lval *a)
{
  LASSERT_TYPE(a, "def", 0, LVAL_QEXPR);

  /* First argument is symbol list */
  lval *syms = a->cell[0];

  /* Ensure all elements of first list are symbols */
  for (int i = 0; i < syms->count; i++)
  {
    LASSERT(a, syms->cell[i]->type == LVAL_SYM, "Function 'def' cannot define non-symbol");
  }

  /* Check correct number of symbols and values */
  LASSERT(a, syms->count == a->count - 1,
          "Function 'def' cannot define incorrect number of values to symbols");

  /* Assign copies of values to symbols */
  for (int i = 0; i < syms->count; i++)
  {
    lenv_put(e, syms->cell[i], a->cell[i + 1]);
  }

  lval_del(a);
  return lval_sexpr();
}

void lenv_add_builtins(lenv *e)
{
  /* List Functions */
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);
  lenv_add_builtin(e, "def",  builtin_def);

  /* Mathematical Functions */
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);
}

int main(int argc, char **argv)
{
  /*Create Some Parsers */
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Symbol = mpc_new("symbol");
  mpc_parser_t *Sexpr = mpc_new("sexpr");
  mpc_parser_t *Qexpr = mpc_new("qexpr");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");

  /* Define them with the following Language */
  mpca_lang(MPCA_LANG_DEFAULT,
            "                                                         \
      number : /-?[0-9]+[.]*[0-9]*/ ;                                 \
      symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;                     \
            sexpr  : '(' <expr>* ')' ;                                      \
      qexpr  : '{' <expr>* '}' ;                                      \
      expr   : <number> | <symbol> | <sexpr> | <qexpr> ;              \
      lispy  : /^/ <expr>* /$/ ;                                      \
      ",
            Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  puts("Lispy version 0.0.0.0.7");
  puts("Press Ctrl+c to Exit\n");

  lenv *e = lenv_new();
  lenv_add_builtins(e);

  while (1)
  {
    char *input = readline("lispy :> ");
    add_history(input);

    /* Attempt to Parse the user Input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r))
    {
      lval *x = lval_eval(e, lval_read(r.output));
      lval_println(x);
      lval_del(x);

      mpc_ast_delete(r.output);
    }
    else
    {
      /* Print the Error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  lenv_del(e);

  /* Undefine and Delete our Parsers */
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  return 0;
}
