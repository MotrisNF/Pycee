#ifndef INTERP_INTERNAL_H
# define INTERP_INTERNAL_H

/*
 * Shared by the files of src/runtime/, src/types/ and src/natives/ only.
 * Functions are prefixed rt_.
 */

#include <stddef.h>
#include <stdint.h>
#include "runtime/interp.h"

/*
 * A native function or method. Its arguments are on the value stack from
 * index base (a method's self is at base, its arguments after it); nargs
 * does not count self. Read them as in->stack[base + i] each time: an
 * allocation may move the stack. call is the CALL node, for errors.
 */
typedef int (*t_native_fn)(t_interp *in, size_t base, int nargs,
                const t_node *call, t_value *out, t_error *err);

/*
 * A method of a built-in type. max -1 = any number of arguments. names,
 * when there are any, are what its arguments are called, in order, so
 * they can be given by name: l.sort(key = f, reverse = true). One left
 * out before one given by name is none.
 */
typedef struct s_method
{
    const char          *name;
    int                 min;
    int                 max;
    t_native_fn         fn;
    const char *const   *names;
}   t_method;

/*
 * What running a statement gives: EXEC_ERROR with err filled, or how it
 * ended. A break or continue goes up through in->jump_levels loops.
 */
typedef enum e_exec
{
    EXEC_ERROR = -1,
    EXEC_OK = 0,
    EXEC_BREAK,
    EXEC_CONTINUE,
    EXEC_RETURN
}   t_exec;

/* interp.c: buffers for showing values, and the value stack */
t_buffer        *rt_buf_take(t_interp *in);
void            rt_buf_give(t_interp *in);
void            rt_push(t_interp *in, t_value v);
t_value         rt_pop(t_interp *in);
t_value         rt_peek(const t_interp *in, size_t distance);

/* errors */
int             rt_error(t_error *err, t_error_type type, const t_node *at,
                    const char *fmt, ...)
                    __attribute__((format(printf, 4, 5)));

/* eval.c */
int             rt_eval(t_interp *in, t_node *n, t_value *out, t_error *err);

/* interp.c: where a variable lives (valid until the next push) */
t_value         *rt_global(t_interp *in, const t_ref *ref);
t_value         *rt_variable(t_interp *in, const t_ref *ref);
t_cell          *rt_box(t_interp *in, size_t index);

/* call.c: functions */
int             rt_make_function(t_interp *in, const t_node *decl,
                    t_value *out);
int             rt_eval_call(t_interp *in, t_node *n, t_value *out,
                    t_error *err);
int             rt_call(t_interp *in, const t_node *call, size_t base,
                    int nargs, t_value *out, t_error *err);
int             rt_call_method_func(t_interp *in, const t_node *call,
                    const t_node *f, const t_value *self, size_t argbase,
                    int npos, const t_node *named, t_value *out,
                    t_error *err);

/* types/object.c: objects of an obj */
int             rt_construct(t_interp *in, const t_node *call,
                    const t_node *cls, size_t argbase, int npos,
                    const t_node *named, t_value *out, t_error *err);
int             rt_get_attr(t_interp *in, const t_node *n, size_t base,
                    t_value *out, t_error *err);
int             rt_set_attr(t_interp *in, const t_node *n, size_t base,
                    t_value v, t_error *err);
int             rt_call_attr(t_interp *in, const t_node *call, size_t base,
                    int npos, const t_node *named, t_value *out,
                    t_error *err);
const t_node    *rt_obj_find(const t_node *cls, t_name name);
const t_node    *rt_special(t_value v, const char *name);
int             rt_call_named(t_interp *in, const t_node *call, size_t base,
                    const char *name, int npos, t_value *out, t_error *err);

/* exec.c: statements */
t_exec          rt_exec(t_interp *in, t_node *n, t_error *err);

/* What a for over v gives, as a new list in *out. */
int             rt_collect(t_interp *in, const t_node *at, t_value v,
                    t_value *out, t_error *err);
t_exec          rt_exec_block(t_interp *in, t_node *block, t_error *err);
int             rt_run_var(t_interp *in, const t_node *n, t_error *err);
int             rt_not_yet(t_error *err, const t_node *n, const char *what);

/* ops.c: operators */
int             rt_unary(t_interp *in, const t_node *n, t_value v,
                    t_value *out, t_error *err);
int             rt_binary(t_interp *in, const t_node *n, t_token_type op,
                    t_value *out, t_error *err);
int             rt_equal(t_value a, t_value b);
int             rt_compare(t_interp *in, const t_node *n, t_value a,
                    t_value b, int *out, t_error *err);
int             rt_truthy(const t_node *n, t_value v, const char *what,
                    int *out, t_error *err);

/* format.c: values as text */
void            rt_format(t_buffer *b, t_value v, int repr);
int             rt_show(t_interp *in, const t_node *at, t_buffer *b,
                    t_value v, int repr, t_error *err);
void            rt_format_float(t_buffer *b, double f);

/* fmtspec.c: f-string fields */
int             rt_format_spec(t_interp *in, t_buffer *out, t_value v,
                    const t_fmt_spec *spec, const t_node *n, t_error *err);

/* methods.c */
int             rt_call_method(t_interp *in, const t_node *call,
                    t_name name, size_t base, int nargs, t_value *out,
                    t_error *err);
int             rt_has_method(t_value v, t_name name);
int             rt_check_arity(const t_node *call, const char *name,
                    int min, int max, int nargs, t_error *err);

/* types/range.c */
uint64_t        rt_range_len(const t_range *r);
int64_t         rt_range_at(const t_range *r, uint64_t i);

/* types/number.c */
extern const t_method   g_number_methods[];

/* types/string.c */
extern const t_method   g_string_methods[];
size_t          rt_string_chars(t_string *s);
size_t          rt_string_offset(t_string *s, size_t i);
t_string        *rt_new_string(t_interp *in, const char *s, size_t len);
int             rt_string_contains(const t_string *s, const t_string *sub);
int             rt_join(t_interp *in, const t_node *call, const t_string *sep,
                    t_value c, t_value *out, t_error *err);

/* types/seq.c: indexes and slices of tuples and strings */
int             rt_index_value(t_interp *in, const t_node *n, size_t base,
                    t_value *out, t_error *err);
int             rt_index_store(t_interp *in, const t_node *n, size_t base,
                    t_value v, t_error *err);
t_value         *rt_seq_items(t_value c, size_t *len);

/* types/error.c */
extern const t_method   g_error_methods[];

/* types/table.c, types/dict.c: the hash table, dict and set */
int             rt_hash(const t_node *n, t_value v, const char *what,
                    uint64_t *out, t_error *err);
int             rt_table_find(const t_dict *d, t_value key, uint64_t hash,
                    size_t *pos);
int             rt_table_set(t_heap *h, t_dict *d, t_value key,
                    uint64_t hash, t_value value);
void            rt_table_remove(t_dict *d, size_t pos);
void            rt_table_clear(t_dict *d);
t_dict          *rt_table_new(t_heap *h, t_obj_type type);
int             rt_table_put(t_interp *in, const t_node *call, size_t slot,
                    t_value key, t_value value, t_error *err);
int             rt_table_has(const t_node *call, t_value table, t_value key,
                    int *out, t_error *err);
int             rt_dict_get(t_interp *in, const t_node *call, t_value d,
                    t_value key, t_value *out, t_error *err);
extern const t_method   g_dict_methods[];
extern const t_method   g_set_methods[];

/* types/list.c */
extern const t_method   g_list_methods[];
extern const t_method   g_tuple_methods[];

/* types/tuple.c */
int             rt_seq_index(const t_node *n, const char *type, size_t len,
                    int64_t i, size_t *out, t_error *err);

/* natives/natives.c */
extern const t_native_fn    g_native_fns[];
extern const int            g_native_arity[][2];
extern const char           *g_native_names[];

/* methods.c */
const t_method  *rt_find_method(t_value v, t_name name);
int             rt_run_method(t_interp *in, const t_node *call,
                    const t_method *m, size_t base, int nargs, t_value *out,
                    t_error *err);

#endif
