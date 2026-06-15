#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

/* ---------- tagged value type ---------- */
typedef enum { T_NUM, T_STR } ValueType;

typedef enum { N_INT, N_FLOAT, N_DOUBLE } NumKind;

typedef struct {
    ValueType type;
    NumKind   nkind;
    double    nval;
    char     *sval;
} Value;

static const char *type_name(ValueType t) {
    return t == T_NUM ? "number" : "string";
}

/* ---------- runtime diagnostics ---------- */
static void rt_warning(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "runtime warning: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}
static void rt_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "runtime error: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

/* ---------- value stack ---------- */
#define STACK_MAX 1024
static Value value_stack[STACK_MAX];
static int   sp = 0;

static void push(Value v) {
    if (sp >= STACK_MAX) rt_error("stack overflow");
    value_stack[sp++] = v;
}
static Value pop(void) {
    if (sp <= 0) rt_error("stack underflow (expression with too few operands)");
    return value_stack[--sp];
}

static Value make_num(double n) {
    Value v; v.type = T_NUM; v.nkind = N_DOUBLE; v.nval = n; v.sval = NULL; return v;
}
static Value make_num_kind(double n, NumKind k) {
    Value v; v.type = T_NUM; v.nkind = k; v.nval = n; v.sval = NULL; return v;
}
static Value make_str(const char *s) {
    Value v; v.type = T_STR; v.nkind = N_DOUBLE; v.nval = 0;
    v.sval = malloc(strlen(s) + 1);
    if (!v.sval) rt_error("out of memory");
    strcpy(v.sval, s);
    return v;
}

/* ---------- name-suffix type rules ---------- */
static int name_is_string(const char *name) {
    size_t n = strlen(name);
    return n > 0 && name[n - 1] == '$';
}
static int name_is_int(const char *name) {
    size_t n = strlen(name);
    return n > 0 && name[n - 1] == '%';
}
static int name_is_double(const char *name) {
    size_t n = strlen(name);
    return n > 0 && name[n - 1] == '#';
}


/* ---------- low-level ops used by generated code ---------- */
void stack_push(int value) { push(make_num((double)value)); }

/* used by JUMP_IF_FALSE: needs a numeric truth value */
int stack_pop(void) {
    Value v = pop();
    if (v.type != T_NUM) {
        rt_warning("expected a number but found a string; treating as 0");
        return 0;
    }
    return v.nval != 0.0;
}

/* ---------- symbol table ---------- */
#define VAR_MAX 512
typedef struct {
    char  *name;
    Value  value;
    int    assigned;
} VarSlot;

static VarSlot vars[VAR_MAX];
static int     var_count = 0;

static VarSlot *find_var(const char *name) {
    for (int i = 0; i < var_count; i++)
        if (strcmp(vars[i].name, name) == 0) return &vars[i];
    return NULL;
}
static VarSlot *intern_var(const char *name) {
    VarSlot *s = find_var(name);
    if (s) return s;
    if (var_count >= VAR_MAX) rt_error("too many variables");
    s = &vars[var_count++];
    s->name = malloc(strlen(name) + 1);
    strcpy(s->name, name);
    s->value = name_is_string(name) ? make_str("") : make_num(0.0);
    s->assigned = 0;
    return s;
}

/* coerce a value to the type implied by a variable name */
static Value coerce_to_var(const char *name, Value v) {
    if (name_is_string(name)) {
        if (v.type != T_STR)
            rt_warning("storing a %s into string variable '%s'", type_name(v.type), name);
        return v.type == T_STR ? make_str(v.sval) : make_str("");
    }
    if (v.type != T_NUM) {
        rt_warning("storing a %s into numeric variable '%s'", type_name(v.type), name);
        return make_num(0.0);
    }
    if (name_is_int(name)) {
        return make_num_kind(round(v.nval), N_INT);
    }
    if (name_is_double(name)) {
        return make_num_kind(v.nval, N_DOUBLE);
    }
    return make_num_kind((double)(float)v.nval, N_FLOAT);
}

/* ---------- loads / stores ---------- */
void LOAD_CONST(int value)       { push(make_num((double)value)); }
void LOAD_NUM(double value)      { push(make_num(value)); }
void LOAD_STR(const char *value) { push(make_str(value)); }

void LOAD_VAR(const char *name) {
    VarSlot *s = intern_var(name);
    if (!s->assigned) {
        rt_warning("variable '%s' used before it was assigned; using %s default",
                   name, name_is_string(name) ? "empty-string" : "0");
        push(name_is_string(name) ? make_str("") : make_num(0.0));
        return;
    }
    push(s->value.type == T_STR ? make_str(s->value.sval) : s->value);
}

void STORE(const char *name) {
    Value v = pop();
    VarSlot *s = intern_var(name);
    s->value = coerce_to_var(name, v);
    s->assigned = 1;
}

/* ---------- arrays (DIM) ----------*/
#define ARR_MAX 128
typedef struct {
    char  *name;
    int    size;
    Value *data;
} Array;
static Array arrays[ARR_MAX];
static int   array_count = 0;

static Array *find_array(const char *name) {
    for (int i = 0; i < array_count; i++)
        if (strcmp(arrays[i].name, name) == 0) return &arrays[i];
    return NULL;
}

/* DIM name(n)  -> allocate elements 0..n inclusive (GW-BASIC is inclusive) */
void DIM(const char *name, int upper) {
    if (find_array(name)) { rt_warning("array '%s' redimensioned", name); return; }
    if (array_count >= ARR_MAX) rt_error("too many arrays");
    int size = upper + 1;
    Array *a = &arrays[array_count++];
    a->name = malloc(strlen(name) + 1); strcpy(a->name, name);
    a->size = size;
    a->data = malloc(sizeof(Value) * size);
    for (int i = 0; i < size; i++)
        a->data[i] = name_is_string(name) ? make_str("") : make_num(0.0);
}

static Array *need_array(const char *name) {
    Array *a = find_array(name);
    if (!a) {
        rt_warning("array '%s' used without DIM; auto-dimensioning to 10", name);
        DIM(name, 10);
        a = find_array(name);
    }
    return a;
}

/* index is popped from the stack at runtime */
void LOAD_ARRAY(const char *name) {
    Value idx = pop();
    if (idx.type != T_NUM) rt_error("array index for '%s' is not a number", name);
    int i = (int)idx.nval;
    Array *a = need_array(name);
    if (i < 0 || i >= a->size) rt_error("array index %d out of bounds for '%s'", i, name);
    push(a->data[i].type == T_STR ? make_str(a->data[i].sval) : a->data[i]);
}
void STORE_ARRAY(const char *name) {
    Value val = pop();
    Value idx = pop();
    if (idx.type != T_NUM) rt_error("array index for '%s' is not a number", name);
    int i = (int)idx.nval;
    Array *a = need_array(name);
    if (i < 0 || i >= a->size) rt_error("array index %d out of bounds for '%s'", i, name);
    a->data[i] = coerce_to_var(name, val);
}

/* ---------- GOSUB / RETURN return-address stack ----------*/
#define CALL_MAX 256
static void *call_stack[CALL_MAX];
static int   call_sp = 0;

void gosub_push(void *addr) {
    if (call_sp >= CALL_MAX) rt_error("GOSUB nested too deeply");
    call_stack[call_sp++] = addr;
}
void *goback(void) {
    if (call_sp <= 0) rt_error("RETURN without GOSUB");
    return call_stack[--call_sp];
}

/* ---------- output ---------- */
void PRINT(void) {
    Value v = pop();
    if (v.type == T_STR) { printf("%s\n", v.sval ? v.sval : ""); return; }
    if (v.nval == floor(v.nval) && fabs(v.nval) < 1e15) {
        printf("%ld\n", (long)v.nval);
        return;
    }
    if (v.nkind == N_DOUBLE)
        printf("%.16g\n", v.nval);
    else
        printf("%.7g\n", v.nval);
}

/* ---------- arithmetic (double) with runtime type checking ---------- */
static double need_num(Value v, const char *op) {
    if (v.type != T_NUM) {
        rt_warning("operator '%s' needs a number but got a %s; using 0", op, type_name(v.type));
        return 0.0;
    }
    return v.nval;
}

void ADD(void) {
    Value b = pop(), a = pop();
    if (a.type == T_STR && b.type == T_STR) {
        size_t len = strlen(a.sval) + strlen(b.sval) + 1;
        char *r = malloc(len);
        strcpy(r, a.sval); strcat(r, b.sval);
        Value v = make_str(r); free(r); push(v); return;
    }
    if (a.type != b.type)
        rt_warning("'+' applied to a %s and a %s", type_name(a.type), type_name(b.type));
    push(make_num(need_num(a, "+") + need_num(b, "+")));
}
void SUB(void) { Value b=pop(),a=pop(); push(make_num(need_num(a,"-") - need_num(b,"-"))); }
void MUL(void) { Value b=pop(),a=pop(); push(make_num(need_num(a,"*") * need_num(b,"*"))); }
void DIV(void) {
    Value b=pop(),a=pop();
    double bb = need_num(b,"/");
    if (bb == 0.0) rt_error("division by zero");
    push(make_num(need_num(a,"/") / bb));
}
void MOD(void) {
    Value b=pop(),a=pop();
    long bb = (long)need_num(b,"MOD");
    if (bb == 0) rt_error("division by zero (MOD)");
    push(make_num((double)((long)need_num(a,"MOD") % bb)));
}

static int cmp_values(Value a, Value b, const char *op) {
    if (a.type == T_STR && b.type == T_STR) return strcmp(a.sval, b.sval);
    if (a.type != b.type)
        rt_warning("comparison '%s' between a %s and a %s", op, type_name(a.type), type_name(b.type));
    double x = need_num(a, op), y = need_num(b, op);
    return (x > y) - (x < y);
}
void CMP_EQ(void){ Value b=pop(),a=pop(); push(make_num(cmp_values(a,b,"=")  == 0)); }
void CMP_NE(void){ Value b=pop(),a=pop(); push(make_num(cmp_values(a,b,"<>") != 0)); }
void CMP_LT(void){ Value b=pop(),a=pop(); push(make_num(cmp_values(a,b,"<")  <  0)); }
void CMP_LE(void){ Value b=pop(),a=pop(); push(make_num(cmp_values(a,b,"<=") <= 0)); }
void CMP_GT(void){ Value b=pop(),a=pop(); push(make_num(cmp_values(a,b,">")  >  0)); }
void CMP_GE(void){ Value b=pop(),a=pop(); push(make_num(cmp_values(a,b,">=") >= 0)); }

#define JUMP(label)          goto label
#define JUMP_IF_FALSE(label) if (!stack_pop()) goto label

int main(void) {
