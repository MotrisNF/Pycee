#ifndef AST_H
# define AST_H

#include <stddef.h>
#include <stdint.h>
#include "front/tokens.h"

/*
 * Node types. X(enum name, name printed by --ast).
 * elif is an IF in the else branch of another IF; "a is not b" is
 * UNARY not over BINARY is. Neither has a node of its own.
 */
# define NODE_TYPES(X) \
    /* Declarations */ \
    X(NODE_PROGRAM,     "Program") \
    X(NODE_IMPORT,      "Import") \
    X(NODE_FUNC,        "Func") \
    X(NODE_PARAM,       "Param") \
    X(NODE_OBJ,         "Obj") \
    X(NODE_VAR,         "Var") \
    X(NODE_MULTI_VAR,   "MultiVar") \
    /* Statements */ \
    X(NODE_BLOCK,       "Block") \
    X(NODE_IF,          "If") \
    X(NODE_WHILE,       "While") \
    X(NODE_DO_WHILE,    "DoWhile") \
    X(NODE_FOR,         "For") \
    X(NODE_SWITCH,      "Switch") \
    X(NODE_TRY,         "Try") \
    X(NODE_EXCEPT,      "Except") \
    X(NODE_RAISE,       "Raise") \
    X(NODE_ASSERT,      "Assert") \
    X(NODE_CASE,        "Case") \
    X(NODE_RETURN,      "Return") \
    X(NODE_BREAK,       "Break") \
    X(NODE_CONTINUE,    "Continue") \
    X(NODE_ASSIGN,      "Assign") \
    X(NODE_MULTI_ASSIGN, "MultiAssign") \
    X(NODE_EXPR_STMT,   "ExprStmt") \
    X(NODE_MAN,         "Man") \
    /* Expressions */ \
    X(NODE_INT,         "Int") \
    X(NODE_FLOAT,       "Float") \
    X(NODE_STRING,      "String") \
    X(NODE_FSTRING,     "FString") \
    X(NODE_FORMAT,      "Format") \
    X(NODE_BOOL,        "Bool") \
    X(NODE_NONE,        "None") \
    X(NODE_SELF,        "Self") \
    X(NODE_SUPER,       "Super") \
    X(NODE_IDENT,       "Ident") \
    X(NODE_LIST,        "List") \
    X(NODE_UNARY,       "Unary") \
    X(NODE_BINARY,      "Binary") \
    X(NODE_TERNARY,     "Ternary") \
    X(NODE_INCDEC,      "IncDec") \
    X(NODE_CALL,        "Call") \
    X(NODE_INDEX,       "Index") \
    X(NODE_FIELD,       "Field") \
    X(NODE_NAMED_ARG,   "NamedArg") \
    X(NODE_SPREAD,      "Spread") \
    X(NODE_PAIR,        "Pair") \
    X(NODE_COMPREHENSION, "Comprehension") \
    X(NODE_COMP_ADD,    "CompAdd")

# define NODE_AS_ENUM(name, str) name,

typedef enum e_node_type
{
    NODE_TYPES(NODE_AS_ENUM)
    NODE_TYPE_COUNT
}   t_node_type;

/* A name inside the arena copy of the source (not '\0'-terminated). */
typedef struct s_name
{
    const char  *ptr;
    size_t      len;
}   t_name;

/*
 * What a name refers to, filled by the resolver (all zero = REF_NONE
 * before it runs). slot is the index of a global variable in its
 * module's global table, of a local in its function's frame, or of a
 * native function; module is the index of the module a global belongs
 * to (or, for REF_MODULE, the module named); decl is the FUNC or OBJ a
 * function or object name refers to. REF_CAPTURE is a variable of an
 * enclosing function: slot is its index in the running closure's
 * captures. The resolver rewrites a qualified
 * name (m.f, a FIELD) into an IDENT f with its ref, keeping the node's
 * offset and len over the whole "m.f".
 */
typedef enum e_ref_kind
{
    REF_NONE,
    REF_GLOBAL,
    REF_LOCAL,
    REF_CAPTURE,
    REF_NATIVE,
    REF_FUNC,
    REF_OBJ,
    REF_MODULE
}   t_ref_kind;

typedef struct s_ref
{
    t_ref_kind          kind;
    int                 slot;
    int                 module;
    const struct s_node *decl;
}   t_ref;

/*
 * A format specifier of an f-string field, as in Python:
 * [[fill]align][sign][#][0][width][grouping][.precision][type]. Checked
 * by the parser. align is '<', '>', '^', '=' or 0 (the default of the
 * value: left for text, right for numbers); sign '+', '-', ' ' or 0;
 * grouping ',' or '_' or 0; precision -1 when absent; type one of
 * "sdboxXeEfFgG%" or 0.
 */
/* What a comprehension makes. */
# define COMP_GENERAL   0
# define COMP_LIST      1
# define COMP_TUPLE     2
# define COMP_SET       3
# define COMP_DICT      4
/* any(... for ...), all(...), sum(...), min(...), max(...) */
# define COMP_ANY       5
# define COMP_ALL       6
# define COMP_SUM       7
# define COMP_MIN       8
# define COMP_MAX       9

typedef struct s_fmt_spec
{
    int32_t fill;
    char    align;
    char    sign;
    char    alt;
    char    zero;
    char    grouping;
    char    type;
    int     width;
    int     precision;
}   t_fmt_spec;

/*
 * A variable an inner function uses from an enclosing one. The closure
 * gets it when it is created: from a slot of the frame creating it
 * (from_local, index = that slot, which holds the variable's cell) or
 * from that frame's own captures (index = position there). decl is the
 * variable's declaration (VAR, PARAM, FOR or FUNC), its identity.
 */
typedef struct s_capture
{
    const struct s_node *decl;
    int                 from_local;
    int                 index;
    struct s_capture    *next;
}   t_capture;

/*
 * Every child list is a linked list through 'next' (a node is in at most
 * one list). offset/len span the source text of the whole node, for
 * error carets and the traceback.
 */
typedef struct s_node
{
    t_node_type     type;
    size_t          offset;
    size_t          len;
    struct s_node   *next;
    union
    {
        /*
         * PROGRAM, BLOCK, LIST. nslots (PROGRAM of a REPL line): frame
         * slots of the for variables of its top-level statements.
         */
        struct { struct s_node *items; int nslots; } list;
        /*
         * IMPORT: "import m [as alias]" (names NULL, alias.ptr NULL if no
         * "as") or "from m import a, b" (is_from, names is a list of IDENT).
         */
        struct { t_name module; t_name alias; struct s_node *names;
                 int is_from; } import;
        /*
         * FUNC (also prog, lambdas and functions declared inside another
         * one): params is a list of PARAM (nparams of them, nrequired
         * without a default, the last one ...rest if has_rest). nslots is
         * the size of its frame: params first (slots 0..n-1), then every
         * var, vemp and for variable of the body, each with a slot of its
         * own. A lambda has no name; expr_body: (x) => expr, body is that
         * expression. Filled by the resolver: captures (ncaptures of them,
         * in order), and for a nested one its slot in the enclosing frame
         * and whether an inner function captures it (captured).
         */
        struct { t_name name; struct s_node *params; struct s_node *body;
                 int is_prog; int nslots; int nparams; int nrequired;
                 int has_rest; int is_lambda; int expr_body; int nested;
                 int slot; int captured; int ncaptures;
                 t_capture *captures; int is_private; int is_static;
                 const struct s_node *cls; struct s_node *self; } func;
        /*
         * PARAM: default_ may be NULL. captured: an inner function uses it,
         * so it lives in a cell (set by the resolver).
         */
        struct { t_name name; int slot; int captured; int is_rest;
                 struct s_node *default_; } param;
        /*
         * OBJ: members is a list of VAR and FUNC. parent is the IDENT of
         * "extends P" / "(P)", or NULL. Filled by the resolver: base, the
         * OBJ it extends (NULL for none or an error type), error_base,
         * the error type it descends from (-1 if none), nfields (its
         * fields and its parents': the parents' come first), init_slots
         * (the frame its field values are computed in), module, and
         * layout (0 not done, 1 being done, 2 done). cname is the name
         * '\0'-terminated (in the arena), for messages and type().
         */
        struct { t_name name; struct s_node *members; struct s_node *parent;
                 const struct s_node *base; int error_base; int nfields;
                 int init_slots; int module; int layout;
                 const char *cname; } obj;
        /*
         * VAR (also vemp and const): at most one of size / init; a const
         * always has init. slot: global slot at top level, frame slot
         * inside a function, unused for obj fields.
         */
        struct { t_name name; int is_vemp; int is_const; int is_global;
                 int captured; int slot; struct s_node *size;
                 struct s_node *init; int is_private; int is_static;
                 const struct s_node *cls; } var;
        /* IF: else_ is a BLOCK, another IF (elif) or NULL */
        struct { struct s_node *cond; struct s_node *then;
                 struct s_node *else_; } if_;
        /* WHILE, DO_WHILE */
        struct { struct s_node *cond; struct s_node *body; } while_;
        /*
         * FOR: slot is the frame slot of the loop variable. With several
         * (for (k, v in ...)), the others are VAR nodes (vemp) in more.
         */
        struct { t_name name; int slot; int captured; struct s_node *more;
                 struct s_node *iterable; struct s_node *body;
                 struct s_node *else_; } for_;
        /* SWITCH: cases is a list of CASE; default_ is a BLOCK or NULL */
        struct { struct s_node *subject; struct s_node *cases;
                 struct s_node *default_; } switch_;
        /*
         * TRY: handlers is a list of EXCEPT (maybe empty if finally_ is
         * there). EXCEPT: types is a list of IDENT naming error types
         * (empty: any error); name.ptr is NULL without "(name)"; slot
         * and captured are the variable's, as for a for variable.
         */
        struct { struct s_node *body; struct s_node *handlers;
                 struct s_node *finally_; } try_;
        struct { struct s_node *types; t_name name; int slot; int captured;
                 struct s_node *body; } except;
        /* RAISE: value is an error. ASSERT: message may be NULL. */
        struct { struct s_node *value; struct s_node *message; } raise;
        /*
         * MAN (only typed alone at the REPL's prompt): what is between the
         * parentheses, as written and never evaluated ("list.len", "+");
         * empty for man().
         */
        struct { t_name topic; } man;
        /* CASE: values is a list of INT / FLOAT / STRING */
        struct { struct s_node *values; struct s_node *body; } case_;
        /*
         * BREAK, CONTINUE: levels is 1, or 2 for "break outer" (the loop
         * that encloses the current one).
         */
        struct { int levels; } jump;
        /* RETURN: value may be NULL */
        struct { struct s_node *value; } ret;
        /* ASSIGN: op is TOK_ASSIGN, TOK_PLUS_EQ... */
        struct { t_token_type op; struct s_node *target;
                 struct s_node *value; } assign;
        /*
         * MULTI_ASSIGN: a, b = x, y (two lists, linked through next).
         * MULTI_VAR: var a, b = x, y; targets are VAR nodes without init.
         * One value alone (a, b = t) is a tuple unpacked when it runs.
         */
        struct { struct s_node *targets; struct s_node *values; } multi;
        struct { struct s_node *expr; } expr_stmt;
        int64_t integer;
        double number;      /* FLOAT */
        /*
         * FSTRING: list.items is its parts, STRING (literal text) and
         * FORMAT ({expr:spec}) nodes, in order.
         */
        struct { struct s_node *expr; t_fmt_spec spec; } format;
        /* STRING: escapes already decoded, '\0'-terminated */
        struct { const char *chars; size_t len; } string;
        int boolean;
        /*
         * IDENT: ref is filled by the resolver. alias: in from m import
         * a as b, the name it gets here (ptr NULL if none).
         */
        struct { t_name name; t_ref ref; t_name alias; } ident;
        /* UNARY: op is TOK_MINUS, TOK_TILDE or TOK_NOT */
        struct { t_token_type op; struct s_node *operand; } unary;
        struct { t_token_type op; struct s_node *left;
                 struct s_node *right; } binary;
        /* TERNARY: cond ? then : else_ */
        struct { struct s_node *cond; struct s_node *then;
                 struct s_node *else_; } ternary;
        /* INCDEC: op is TOK_PLUS_PLUS or TOK_MINUS_MINUS */
        struct { t_token_type op; int prefix; struct s_node *target; } incdec;
        struct { struct s_node *callee; struct s_node *args; } call;
        /*
         * INDEX: c[index], or with slice c[index:end:step], where each
         * part may be NULL (c[:], c[::-1]).
         */
        struct { struct s_node *object; struct s_node *index;
                 struct s_node *end; struct s_node *step; int slice; } index;
        /*
         * FIELD: object.name. member: what the resolver found it is (a
         * field of self, a static member, a method of super), or NULL to
         * look it up when it runs; slot: the index of a field of self.
         */
        struct { struct s_node *object; t_name name;
                 const struct s_node *member; int slot; } field;
        /*
         * NAMED_ARG: f(name = value). index: the parameter it fills, when
         * the resolver knows the function (-1 otherwise).
         */
        struct { t_name name; struct s_node *value; int index; } named;
        /* SPREAD: f(...t), the items of a tuple as arguments */
        struct { struct s_node *value; } spread;
        /*
         * COMPREHENSION: [x * 2 for (x in l) if (x > 0)] and the same
         * inside tuple(...), set(...), dict(k: v for ...). kind is what it
         * makes (COMP_GENERAL until the resolver sees which constructor it
         * is in). loop is a FOR whose body adds (COMP_ADD, add) each value
         * to the result, which grows in frame slot slot meanwhile.
         */
        struct { int kind; int slot; struct s_node *loop;
                 struct s_node *add; } comp;
        /* PAIR: key: value, only inside dict(...) */
        struct { struct s_node *key; struct s_node *value; } pair;
    } as;
}   t_node;

const char  *node_type_name(t_node_type type);

/* Prints the tree to stdout, one node per line, indented by depth. */
void        ast_print(const t_node *node);

#endif
