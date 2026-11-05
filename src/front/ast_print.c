#include <stdio.h>
#include "core/stack.h"
#include "front/ast.h"

#define NODE_AS_NAME(name, str) [name] = str,

static const char   *g_node_names[NODE_TYPE_COUNT] = {
    NODE_TYPES(NODE_AS_NAME)
};

const char  *node_type_name(t_node_type type)
{
    return (g_node_names[type]);
}

static void print_indent(int depth)
{
    while (depth-- > 0)
        fputs("  ", stdout);
}

static void print_string(const char *s, size_t len)
{
    size_t  i;

    putchar('"');
    for (i = 0; i < len; i++)
    {
        if (s[i] == '\n')
            fputs("\\n", stdout);
        else if (s[i] == '\t')
            fputs("\\t", stdout);
        else if (s[i] == '"' || s[i] == '\\')
            printf("\\%c", s[i]);
        else
            putchar(s[i]);
    }
    putchar('"');
}

static void print_node(const t_node *n, int depth);

/* Prints a labelled child ("then:", "else:"...) one level deeper. */
static void print_child(const char *label, const t_node *n, int depth)
{
    if (!n)
        return ;
    print_indent(depth);
    printf("%s:\n", label);
    print_node(n, depth + 1);
}

static void print_list(const char *label, const t_node *n, int depth)
{
    if (!n)
        return ;
    if (label)
    {
        print_indent(depth);
        printf("%s:\n", label);
        depth++;
    }
    for (; n; n = n->next)
        print_node(n, depth);
}

/* Past the stack guard the rest of that branch is elided, not printed. */
static void print_node(const t_node *n, int depth)
{
    print_indent(depth);
    if (stack_exhausted())
    {
        puts("...");
        return ;
    }
    fputs(node_type_name(n->type), stdout);
    switch (n->type)
    {
        case NODE_PROGRAM:
        case NODE_BLOCK:
        case NODE_LIST:
        case NODE_FSTRING:
            putchar('\n');
            print_list(NULL, n->as.list.items, depth + 1);
            break ;
        case NODE_IMPORT:
            if (n->as.import.is_from)
                printf(" from %.*s\n", (int)n->as.import.module.len,
                    n->as.import.module.ptr);
            else if (n->as.import.alias.ptr)
                printf(" %.*s as %.*s\n", (int)n->as.import.module.len,
                    n->as.import.module.ptr, (int)n->as.import.alias.len,
                    n->as.import.alias.ptr);
            else
                printf(" %.*s\n", (int)n->as.import.module.len,
                    n->as.import.module.ptr);
            print_list(NULL, n->as.import.names, depth + 1);
            break ;
        case NODE_FUNC:
            if (n->as.func.is_lambda)
                printf(" lambda%s\n", n->as.func.expr_body ? " =>" : "");
            else
                printf(" %s%s%s %.*s\n", n->as.func.is_private ? "private "
                    : "", n->as.func.is_static ? "static " : "",
                    n->as.func.is_prog ? "prog" : "func",
                    (int)n->as.func.name.len, n->as.func.name.ptr);
            print_list("params", n->as.func.params, depth + 1);
            print_child("body", n->as.func.body, depth + 1);
            break ;
        case NODE_PARAM:
            printf(" %s%.*s\n", n->as.param.is_rest ? "..." : "",
                (int)n->as.param.name.len, n->as.param.name.ptr);
            print_child("default", n->as.param.default_, depth + 1);
            break ;
        case NODE_NAMED_ARG:
            printf(" %.*s\n", (int)n->as.named.name.len, n->as.named.name.ptr);
            print_node(n->as.named.value, depth + 1);
            break ;
        case NODE_COMPREHENSION:
            printf(" %s\n", (const char *[]){"general", "list", "tuple", "set",
                "dict"}[n->as.comp.kind]);
            print_node(n->as.comp.loop, depth + 1);
            break ;
        case NODE_COMP_ADD:
            putchar('\n');
            print_node(n->as.comp.loop, depth + 1);
            break ;
        case NODE_PAIR:
            putchar('\n');
            print_node(n->as.pair.key, depth + 1);
            print_node(n->as.pair.value, depth + 1);
            break ;
        case NODE_SPREAD:
            putchar('\n');
            print_node(n->as.spread.value, depth + 1);
            break ;
        case NODE_OBJ:
            printf(" %.*s", (int)n->as.obj.name.len, n->as.obj.name.ptr);
            if (n->as.obj.parent)
                printf(" extends %.*s", (int)n->as.obj.parent->as.ident.name.len,
                    n->as.obj.parent->as.ident.name.ptr);
            putchar('\n');
            print_list(NULL, n->as.obj.members, depth + 1);
            break ;
        case NODE_VAR:
            printf(" %s%s%s %.*s\n", n->as.var.is_private ? "private " : "",
                n->as.var.is_static ? "static " : "", n->as.var.is_const
                ? "const" : n->as.var.is_vemp ? "vemp" : "var",
                (int)n->as.var.name.len, n->as.var.name.ptr);
            print_child("size", n->as.var.size, depth + 1);
            print_child("init", n->as.var.init, depth + 1);
            break ;
        case NODE_IF:
            putchar('\n');
            print_child("cond", n->as.if_.cond, depth + 1);
            print_child("then", n->as.if_.then, depth + 1);
            print_child("else", n->as.if_.else_, depth + 1);
            break ;
        case NODE_WHILE:
        case NODE_DO_WHILE:
            putchar('\n');
            print_child("cond", n->as.while_.cond, depth + 1);
            print_child("body", n->as.while_.body, depth + 1);
            break ;
        case NODE_FOR:
            printf(" %.*s\n", (int)n->as.for_.name.len, n->as.for_.name.ptr);
            print_list("more", n->as.for_.more, depth + 1);
            print_child("in", n->as.for_.iterable, depth + 1);
            print_child("body", n->as.for_.body, depth + 1);
            print_child("else", n->as.for_.else_, depth + 1);
            break ;
        case NODE_SWITCH:
            putchar('\n');
            print_child("subject", n->as.switch_.subject, depth + 1);
            print_list(NULL, n->as.switch_.cases, depth + 1);
            print_child("default", n->as.switch_.default_, depth + 1);
            break ;
        case NODE_CASE:
            putchar('\n');
            print_list("values", n->as.case_.values, depth + 1);
            print_child("body", n->as.case_.body, depth + 1);
            break ;
        case NODE_RETURN:
            putchar('\n');
            if (n->as.ret.value)
                print_node(n->as.ret.value, depth + 1);
            break ;
        case NODE_ASSIGN:
            printf(" %s\n", token_type_desc(n->as.assign.op));
            print_node(n->as.assign.target, depth + 1);
            print_node(n->as.assign.value, depth + 1);
            break ;
        case NODE_BREAK:
        case NODE_CONTINUE:
            puts(n->as.jump.levels == 2 ? " outer" : "");
            break ;
        case NODE_MULTI_ASSIGN:
        case NODE_MULTI_VAR:
            putchar('\n');
            print_list("targets", n->as.multi.targets, depth + 1);
            print_list("values", n->as.multi.values, depth + 1);
            break ;
        case NODE_TRY:
            putchar('\n');
            print_child("body", n->as.try_.body, depth + 1);
            print_list(NULL, n->as.try_.handlers, depth + 1);
            print_child("finally", n->as.try_.finally_, depth + 1);
            break ;
        case NODE_EXCEPT:
            if (n->as.except.name.ptr)
                printf(" (%.*s)", (int)n->as.except.name.len,
                    n->as.except.name.ptr);
            putchar('\n');
            print_list("types", n->as.except.types, depth + 1);
            print_child("body", n->as.except.body, depth + 1);
            break ;
        case NODE_RAISE:
        case NODE_ASSERT:
            putchar('\n');
            print_node(n->as.raise.value, depth + 1);
            if (n->as.raise.message)
                print_node(n->as.raise.message, depth + 1);
            break ;
        case NODE_FORMAT:
            if (n->as.format.spec.type || n->as.format.spec.width
                || n->as.format.spec.precision >= 0
                || n->as.format.spec.align || n->as.format.spec.grouping)
                printf(" align=%c width=%d prec=%d type=%c",
                    n->as.format.spec.align ? n->as.format.spec.align : '-',
                    n->as.format.spec.width, n->as.format.spec.precision,
                    n->as.format.spec.type ? n->as.format.spec.type : '-');
            putchar('\n');
            print_node(n->as.format.expr, depth + 1);
            break ;
        case NODE_EXPR_STMT:
            putchar('\n');
            print_node(n->as.expr_stmt.expr, depth + 1);
            break ;
        case NODE_INT:
            printf(" %lld\n", (long long)n->as.integer);
            break ;
        case NODE_FLOAT:
            printf(" %.17g\n", n->as.number);
            break ;
        case NODE_STRING:
            putchar(' ');
            print_string(n->as.string.chars, n->as.string.len);
            putchar('\n');
            break ;
        case NODE_BOOL:
            printf(" %s\n", n->as.boolean ? "true" : "false");
            break ;
        case NODE_IDENT:
            printf(" %.*s", (int)n->as.ident.name.len, n->as.ident.name.ptr);
            if (n->as.ident.alias.ptr)
                printf(" as %.*s", (int)n->as.ident.alias.len,
                    n->as.ident.alias.ptr);
            putchar('\n');
            break ;
        case NODE_UNARY:
            printf(" %s\n", token_type_desc(n->as.unary.op));
            print_node(n->as.unary.operand, depth + 1);
            break ;
        case NODE_BINARY:
            printf(" %s\n", token_type_desc(n->as.binary.op));
            print_node(n->as.binary.left, depth + 1);
            print_node(n->as.binary.right, depth + 1);
            break ;
        case NODE_TERNARY:
            putchar('\n');
            print_child("cond", n->as.ternary.cond, depth + 1);
            print_child("then", n->as.ternary.then, depth + 1);
            print_child("else", n->as.ternary.else_, depth + 1);
            break ;
        case NODE_INCDEC:
            printf(" %s %s\n", token_type_desc(n->as.incdec.op),
                n->as.incdec.prefix ? "prefix" : "postfix");
            print_node(n->as.incdec.target, depth + 1);
            break ;
        case NODE_CALL:
            putchar('\n');
            print_child("callee", n->as.call.callee, depth + 1);
            print_list("args", n->as.call.args, depth + 1);
            break ;
        case NODE_INDEX:
            puts(n->as.index.slice ? " slice" : "");
            print_node(n->as.index.object, depth + 1);
            if (!n->as.index.slice)
                print_node(n->as.index.index, depth + 1);
            print_child("start", n->as.index.slice ? n->as.index.index : NULL,
                depth + 1);
            print_child("end", n->as.index.end, depth + 1);
            print_child("step", n->as.index.step, depth + 1);
            break ;
        case NODE_FIELD:
            printf(" .%.*s\n", (int)n->as.field.name.len, n->as.field.name.ptr);
            print_node(n->as.field.object, depth + 1);
            break ;
        default:
            putchar('\n');
            break ;
    }
}

void    ast_print(const t_node *node)
{
    print_node(node, 0);
}
