#ifndef MODULES_H
# define MODULES_H

/*
 * Shared by the files of src/natives/ only: the global natives
 * (natives.c) and one file per module of the language (sys.c, math.c...).
 * Every native has the signature of t_native_fn: its arguments are
 * ARG(0)... (read each time: an allocation may move the stack), and the
 * keyword-only ones, when it has any, KW(0)... (none when not given).
 */

# include "resolver/natives.h"
# include "runtime/interp_internal.h"

# define ARG(i) (in->stack[base + (size_t)(i)])
# define KW(i)  (in->stack[base + (size_t)nargs + (size_t)(i)])

/*
 * The value of a module constant, made when the module is first
 * imported (stored at once into its global: nothing else is allocated
 * meanwhile).
 */
typedef t_value (*t_native_const_fn)(t_interp *in);

# define NATIVE_MEMBER_PROTO(mod, name, min, max, names, kw) \
    int nat_##mod##_##name(t_interp *in, size_t base, int nargs, \
        const t_node *call, t_value *out, t_error *err);
# define NATIVE_CONST_PROTO(mod, name) \
    t_value nconst_##mod##_##name(t_interp *in);

NATIVE_MEMBERS(NATIVE_MEMBER_PROTO, NATIVE_CONST_PROTO)

/* natives.c: helpers shared by the modules */
int     rt_int_arg(const t_node *call, t_value v, const char *fname,
            const char *what, int64_t *out, t_error *err);
int     rt_exit(t_interp *in, size_t base, int nargs, const t_node *call,
            t_error *err);

/* A member whose step has not come yet: NotImplementedError. */
# define NATIVE_NOT_YET(mod, name) \
    int nat_##mod##_##name(t_interp *in, size_t base, int nargs, \
        const t_node *call, t_value *out, t_error *err) \
    { \
        (void)in; \
        (void)base; \
        (void)nargs; \
        (void)out; \
        return (rt_error(err, ERR_NOT_IMPLEMENTED, call, \
                #mod "." #name "() is not implemented yet")); \
    }

/* A constant whose step has not come yet: none meanwhile. */
# define NATIVE_CONST_NOT_YET(mod, name) \
    t_value nconst_##mod##_##name(t_interp *in) \
    { \
        (void)in; \
        return (val_none()); \
    }

#endif
