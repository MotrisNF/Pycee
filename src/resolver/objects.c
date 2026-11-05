/*
 * Objects: what an obj extends, where each field goes, its static
 * members, and the names reached through self, super and the obj's name.
 */

#include "core/errors.h"
#include "resolver/resolver_internal.h"

static t_name   member_name(const t_node *m)
{
    if (m->type == NODE_VAR)
        return (m->as.var.name);
    return (m->as.func.name);
}

static int  member_static(const t_node *m)
{
    if (m->type == NODE_VAR)
        return (m->as.var.is_static);
    return (m->as.func.is_static);
}

/* The obj that declares member m. */
const t_node    *rs_member_cls(const t_node *m)
{
    if (m->type == NODE_VAR)
        return (m->as.var.cls);
    return (m->as.func.cls);
}

static int  member_private(const t_node *m)
{
    if (m->type == NODE_VAR)
        return (m->as.var.is_private);
    return (m->as.func.is_private);
}

/* A member of obj or of what it extends (the nearest one), or NULL. */
const t_node    *rs_obj_find(const t_node *obj, t_name name)
{
    const t_node    *m;

    for (; obj; obj = obj->as.obj.base)
    {
        m = rs_find_member(obj, name);
        if (m)
            return (m);
    }
    return (NULL);
}

/*
 * What obj extends: another obj (of this module or imported) or an error
 * type, whose methods and place among the errors it takes.
 */
static int  resolve_parent(t_ctx *c, t_node *obj)
{
    const t_symbol  *sym;
    t_name          name;

    obj->as.obj.error_base = -1;
    if (!obj->as.obj.parent)
        return (0);
    name = obj->as.obj.parent->as.ident.name;
    sym = rs_lookup(c, name);
    if (sym && sym->kind == SYM_OBJ)
    {
        obj->as.obj.base = sym->decl;
        return (0);
    }
    if (sym && sym->kind == SYM_NATIVE && error_type_named(name.ptr, name.len)
        != -1)
    {
        obj->as.obj.error_base = error_type_named(name.ptr, name.len);
        return (0);
    }
    return (error_set(c->err, sym ? ERR_TYPE : ERR_NAME,
            obj->as.obj.parent->offset, name.len, sym ? "'%.*s' is not an obj"
            " or an error type: an obj can only extend one of those"
            : "name '%.*s' is not defined", (int)name.len, name.ptr));
}

/* A member may override a method of a parent, but nothing else. */
static int  check_override(t_ctx *c, const t_node *obj, const t_node *m)
{
    const t_node    *prev;
    t_name          name;

    name = member_name(m);
    if (rs_is_named(name, "type") || rs_is_named(name, "is_type"))
        return (error_set(c->err, ERR_SYNTAX, rs_name_offset(c, name),
                name.len, "'%.*s' cannot be redefined: every value has it",
                (int)name.len, name.ptr));
    prev = obj->as.obj.base ? rs_obj_find(obj->as.obj.base, name) : NULL;
    if (!prev || (prev->type == NODE_FUNC && m->type == NODE_FUNC
            && prev->as.func.is_static == m->as.func.is_static))
        return (0);
    return (error_set(c->err, ERR_NAME, rs_name_offset(c, name), name.len,
            "'%.*s' is already a %s of '%.*s', which this obj extends; only"
            " a method can be redefined, by another method", (int)name.len,
            name.ptr, prev->type == NODE_VAR ? "field" : "method",
            (int)rs_member_cls(prev)->as.obj.name.len,
            rs_member_cls(prev)->as.obj.name.ptr));
}

static void collect_fields(t_ctx *c, t_node *obj, t_node *stmt);

/* The fields created by self.name = ... in a list of statements. */
static void collect_list(t_ctx *c, t_node *obj, t_node *n)
{
    for (; n; n = n->next)
        collect_fields(c, obj, n);
}

/*
 * self.name = value in create declares the field name (decided by the
 * author: fields are declared there, the other methods only change
 * them). One that obj (or what it extends) already has is assigned, not
 * declared again. A new field becomes a VAR member without a value, as
 * if it had been written with var.
 */
static void declare_field(t_ctx *c, t_node *obj, const t_node *target)
{
    t_node  *field;
    t_node  **tail;

    if (target->type != NODE_FIELD
        || target->as.field.object->type != NODE_SELF
        || rs_obj_find(obj, target->as.field.name))
        return ;
    field = arena_alloc(c->r->arena, sizeof(t_node));
    field->type = NODE_VAR;
    field->offset = target->offset;
    field->len = target->len;
    field->as.var.name = target->as.field.name;
    field->as.var.cls = obj;
    for (tail = &obj->as.obj.members; *tail; tail = &(*tail)->next)
        ;
    *tail = field;
}

/*
 * Walks the statements of create (into blocks, ifs, loops, switches and
 * try, not into inner functions: they run later) for self.x = ...
 */
static void collect_fields(t_ctx *c, t_node *obj, t_node *n)
{
    t_node  *t;

    if (n->type == NODE_ASSIGN && n->as.assign.op == TOK_ASSIGN)
        declare_field(c, obj, n->as.assign.target);
    else if (n->type == NODE_MULTI_ASSIGN)
        for (t = n->as.multi.targets; t; t = t->next)
            declare_field(c, obj, t);
    else if (n->type == NODE_BLOCK)
        collect_list(c, obj, n->as.list.items);
    else if (n->type == NODE_IF)
    {
        collect_fields(c, obj, n->as.if_.then);
        if (n->as.if_.else_)
            collect_fields(c, obj, n->as.if_.else_);
    }
    else if (n->type == NODE_WHILE || n->type == NODE_DO_WHILE)
        collect_fields(c, obj, n->as.while_.body);
    else if (n->type == NODE_FOR)
    {
        collect_fields(c, obj, n->as.for_.body);
        if (n->as.for_.else_)
            collect_fields(c, obj, n->as.for_.else_);
    }
    else if (n->type == NODE_SWITCH)
    {
        for (t = n->as.switch_.cases; t; t = t->next)
            collect_fields(c, obj, t->as.case_.body);
        if (n->as.switch_.default_)
            collect_fields(c, obj, n->as.switch_.default_);
    }
    else if (n->type == NODE_TRY)
    {
        collect_fields(c, obj, n->as.try_.body);
        for (t = n->as.try_.handlers; t; t = t->next)
            collect_fields(c, obj, t->as.except.body);
        if (n->as.try_.finally_)
            collect_fields(c, obj, n->as.try_.finally_);
    }
}

/* The fields obj's own create declares (an inherited create declared its own). */
static void fields_of_create(t_ctx *c, t_node *obj)
{
    t_node  *m;

    for (m = obj->as.obj.members; m; m = m->next)
        if (m->type == NODE_FUNC && !m->as.func.is_static
            && rs_is_named(m->as.func.name, "create"))
            collect_fields(c, obj, m->as.func.body);
}

/*
 * The layout of obj, its parents first: each field gets the next index
 * after the parents' fields (so a parent's method finds its fields at the
 * same place in a child), and each static var a hidden global slot of
 * the module. Going round to an obj not finished is a cycle.
 */
int rs_obj_layout(t_ctx *c, t_node *obj)
{
    t_node  *m;

    if (obj->as.obj.layout == 2)
        return (0);
    if (obj->as.obj.layout == 1)
        return (error_set(c->err, ERR_SYNTAX, rs_name_offset(c,
                    obj->as.obj.name), obj->as.obj.name.len, "'%.*s' extends"
                " itself (through what it extends)",
                (int)obj->as.obj.name.len, obj->as.obj.name.ptr));
    obj->as.obj.layout = 1;
    obj->as.obj.module = c->mod->index;
    if (resolve_parent(c, obj) == -1 || (obj->as.obj.base
            && rs_obj_layout(c, (t_node *)obj->as.obj.base) == -1))
        return (-1);
    if (obj->as.obj.base)
    {
        obj->as.obj.nfields = obj->as.obj.base->as.obj.nfields;
        obj->as.obj.error_base = obj->as.obj.base->as.obj.error_base;
    }
    fields_of_create(c, obj);
    for (m = obj->as.obj.members; m; m = m->next)
    {
        if (check_override(c, obj, m) == -1)
            return (-1);
        if (m->type != NODE_VAR)
            continue ;
        if (m->as.var.is_static)
        {
            m->as.var.is_global = 1;
            m->as.var.slot = c->mod->nglobals++;
        }
        else
            m->as.var.slot = obj->as.obj.nfields++;
    }
    obj->as.obj.layout = 2;
    return (0);
}

/* How many arguments Name(...) takes: its create's (its own or inherited). */
t_arity rs_obj_arity(const t_node *obj)
{
    const t_node    *create;
    t_name          name;
    t_arity         arity;

    name.ptr = "create";
    name.len = 6;
    create = rs_obj_find(obj, name);
    if (create && create->type == NODE_FUNC)
        return (rs_func_arity(create));
    arity.min = 0;
    arity.max = obj->as.obj.error_base != -1 ? 1 : 0;
    return (arity);
}

/*
 * A member reached from inside cls (self.name, super.name): it must
 * exist, unless cls is an error (its methods, message()..., are found
 * when it runs), and a private one is only for the obj that declares it.
 * *m is NULL for one of the error's.
 */
int rs_obj_member(t_ctx *c, const t_node *cls, t_name name, size_t offset,
        const t_node **m)
{
    *m = rs_obj_find(cls, name);
    if (!*m && cls->as.obj.error_base != -1)
        return (0);
    if (!*m)
        return (error_set(c->err, ERR_ATTRIBUTE, offset, name.len,
                "'%.*s' object has no attribute '%.*s'",
                (int)cls->as.obj.name.len, cls->as.obj.name.ptr,
                (int)name.len, name.ptr));
    if (member_private(*m) && rs_member_cls(*m) != c->obj)
        return (error_set(c->err, ERR_ATTRIBUTE, offset, name.len,
                "'%.*s' is private to '%.*s'", (int)name.len, name.ptr,
                (int)rs_member_cls(*m)->as.obj.name.len,
                rs_member_cls(*m)->as.obj.name.ptr));
    return (0);
}

/*
 * Name.member (Name an obj): only its static members can be reached this
 * way (from outside, the public ones). A static var becomes the global
 * it lives in; *m is what it is.
 */
int rs_obj_static(t_ctx *c, t_node *field, const t_node *cls,
        const t_node **m)
{
    t_name  name;
    size_t  offset;

    name = field->as.field.name;
    offset = rs_name_offset(c, name);
    *m = rs_obj_find(cls, name);
    if (!*m)
        return (error_set(c->err, ERR_ATTRIBUTE, offset, name.len,
                "'%.*s' has no static member '%.*s'", (int)cls->as.obj.name.len,
                cls->as.obj.name.ptr, (int)name.len, name.ptr));
    if (!member_static(*m))
        return (error_set(c->err, ERR_TYPE, offset, name.len, "'%.*s' is not"
                " static: it belongs to each object, reach it through one",
                (int)name.len, name.ptr));
    if (member_private(*m) && rs_member_cls(*m) != c->obj)
        return (error_set(c->err, ERR_ATTRIBUTE, offset, name.len,
                "'%.*s' is private to '%.*s'", (int)name.len, name.ptr,
                (int)rs_member_cls(*m)->as.obj.name.len,
                rs_member_cls(*m)->as.obj.name.ptr));
    if ((*m)->type == NODE_VAR)
    {
        field->type = NODE_IDENT;
        field->as.ident.name = name;
        field->as.ident.ref.kind = REF_GLOBAL;
        field->as.ident.ref.slot = (*m)->as.var.slot;
        field->as.ident.ref.module = rs_member_cls(*m)->as.obj.module;
        field->as.ident.ref.decl = *m;
    }
    else
    {
        field->type = NODE_IDENT;
        field->as.ident.name = name;
        field->as.ident.ref.kind = REF_FUNC;
        field->as.ident.ref.decl = *m;
    }
    return (0);
}
