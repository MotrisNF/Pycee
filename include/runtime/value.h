#ifndef VALUE_H
# define VALUE_H

#include <stddef.h>
#include <stdint.h>
#include "core/errors.h"

/*
 * A runtime value: a tag and, for the immediate kinds, the value itself.
 * Numbers and bools never allocate. Everything else (strings, and later
 * lists, tuples, sets, dicts, functions, objects...) is a heap object
 * reached through obj. Variables do not have a type: values do, so a
 * variable can hold 1 and later a list.
 *
 * int and float are both of type "number" for the language (docs/TYPES.md);
 * they are separate tags so that ints stay exact.
 */
typedef enum e_value_type
{
    VAL_NONE,
    VAL_BOOL,
    VAL_INT,
    VAL_FLOAT,
    VAL_OBJ
}   t_value_type;

typedef struct s_obj    t_obj;
struct s_node;
struct s_method;

typedef struct s_value
{
    t_value_type    type;
    union
    {
        int         boolean;
        int64_t     integer;
        double      number;
        t_obj       *obj;
    } as;
}   t_value;

/*
 * Kinds of heap object. Every object starts with a t_obj header, so the
 * collector (phase 8) can walk and free any of them without knowing what
 * they are: adding a type is adding its struct and its entries in the
 * mark and free functions.
 */
typedef enum e_obj_type
{
    OBJ_STRING,
    OBJ_RANGE,
    OBJ_TUPLE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_BOUND,
    OBJ_CELL,
    OBJ_ERROR,
    OBJ_LIST,
    OBJ_DICT,
    OBJ_SET,
    OBJ_INSTANCE,
    OBJ_CLASS,
    OBJ_METHOD
}   t_obj_type;

/*
 * The common header. next links every object of the heap (the sweep
 * walks that list); marked is for the mark phase. id is what x.id()
 * gives: 1, 2, 3... in order of creation, never reused (not the address,
 * which says where memory is and comes back once an object is freed).
 */
struct s_obj
{
    t_obj_type      type;
    unsigned char   marked;
    uint64_t        id;
    struct s_obj    *next;
};

/*
 * An immutable string, always valid UTF-8 (whatever comes from outside
 * is cleaned on the way in). len counts bytes; nchars counts characters,
 * SIZE_MAX until it is first needed (rt_string_chars); nchars == len
 * means it is all ASCII, where an index is a byte offset. chars has len
 * bytes plus a '\0' that is never part of the text (a string may contain
 * '\0', so len is always used, never strlen).
 */
typedef struct s_string
{
    t_obj   obj;
    size_t  len;
    size_t  nchars;
    char    chars[];
}   t_string;

/*
 * range(start, stop, step), as in Python: the numbers are produced while
 * it is walked, never stored. step is never 0.
 */
typedef struct s_range
{
    t_obj   obj;
    int64_t start;
    int64_t stop;
    int64_t step;
}   t_range;

/*
 * A mutable sequence: len values in items, room for cap (items is its
 * own allocation, freed with the list; it only grows once realloc has
 * succeeded).
 */
typedef struct s_list
{
    t_obj   obj;
    size_t  len;
    size_t  cap;
    t_value *items;
}   t_list;

/*
 * A dict (OBJ_DICT) or a set (OBJ_SET: its values are unused). entries
 * are kept in insertion order, nentries of them (removed ones stay, with
 * used 0, until the table is rebuilt), room for cap; len live ones.
 * index is an open-addressing table of index_cap slots (a power of two,
 * never more than half full): entry position + 1, 0 = empty. Both are
 * allocations of their own, freed with the table.
 */
typedef struct s_entry
{
    t_value     key;
    t_value     value;
    uint64_t    hash;
    int         used;
}   t_entry;

typedef struct s_dict
{
    t_obj   obj;
    size_t  len;
    size_t  nentries;
    size_t  cap;
    t_entry *entries;
    size_t  *index;
    size_t  index_cap;
}   t_dict;

/*
 * An object of an obj (cls, its OBJ node): its fields, in the order the
 * resolver gave them (the parents' first). error: for an obj that
 * extends an error type, the error it is (message, where...), NULL
 * otherwise.
 */
typedef struct s_instance
{
    t_obj               obj;
    const struct s_node *cls;
    struct s_error_value *error;
    size_t              nfields;
    t_value             fields[];
}   t_instance;

/* An obj as a value (Point: calling it builds one). */
typedef struct s_class
{
    t_obj               obj;
    const struct s_node *decl;
}   t_class;

/* A method of an obj taken without calling it, bound to its object. */
typedef struct s_method_value
{
    t_obj               obj;
    t_value             self;
    const struct s_node *func;
}   t_method_value;

/* An immutable sequence of len values. */
typedef struct s_tuple
{
    t_obj   obj;
    size_t  len;
    t_value items[];
}   t_tuple;

/*
 * A variable that an inner function captures: it lives here, not in its
 * frame slot (which holds the cell), so it outlives the frame and every
 * closure that captured it shares it. Never a value of the language.
 */
typedef struct s_cell
{
    t_obj   obj;
    t_value value;
}   t_cell;

/*
 * A func or a lambda as a value: its declaration (a FUNC node) and the
 * cells of the variables it captured, in the order of its captures.
 */
typedef struct s_function
{
    t_obj               obj;
    const struct s_node *decl;
    size_t              ncells;
    t_cell              *cells[];
}   t_function;

/* A native function as a value: its slot in NATIVE_LIST. */
typedef struct s_native
{
    t_obj   obj;
    int     slot;
}   t_native;

/* A method taken without calling it, bound to its value: x.round. */
typedef struct s_bound
{
    t_obj                   obj;
    t_value                 self;
    const struct s_method   *method;
}   t_bound;

/*
 * An error as a value: made by its constructor (ValueError("x")), or
 * caught by except. err holds all of it: type, message, where it
 * happened (where it was made, until it is raised) and its traceback.
 * raised: it has been raised, so raise e keeps where it came from.
 */
typedef struct s_error_value
{
    t_obj   obj;
    t_error err;
    int     raised;
}   t_error_value;

t_value     val_none(void);
t_value     val_bool(int b);
t_value     val_int(int64_t i);
t_value     val_float(double f);
t_value     val_obj(t_obj *o);

int         val_is_string(t_value v);
t_string    *val_string(t_value v);
int         val_is_range(t_value v);
t_range     *val_range(t_value v);
int         val_is_tuple(t_value v);
t_tuple     *val_tuple(t_value v);
int         val_is_obj(t_value v, t_obj_type type);
int         val_is_list(t_value v);
t_list      *val_list(t_value v);
int         val_is_dict(t_value v);
int         val_is_set(t_value v);
t_dict      *val_dict(t_value v);

/* The language name of a value's type: "number", "string", "bool"... */
const char  *val_type_name(t_value v);

#endif
