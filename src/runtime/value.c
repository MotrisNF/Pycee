#include "front/ast.h"
#include "runtime/value.h"

t_value     val_none(void)
{
    t_value v;

    v.type = VAL_NONE;
    v.as.integer = 0;
    return (v);
}

t_value     val_bool(int b)
{
    t_value v;

    v.type = VAL_BOOL;
    v.as.integer = 0;
    v.as.boolean = b != 0;
    return (v);
}

t_value     val_int(int64_t i)
{
    t_value v;

    v.type = VAL_INT;
    v.as.integer = i;
    return (v);
}

t_value     val_float(double f)
{
    t_value v;

    v.type = VAL_FLOAT;
    v.as.number = f;
    return (v);
}

t_value     val_obj(t_obj *o)
{
    t_value v;

    v.type = VAL_OBJ;
    v.as.obj = o;
    return (v);
}

int val_is_string(t_value v)
{
    return (v.type == VAL_OBJ && v.as.obj->type == OBJ_STRING);
}

t_string    *val_string(t_value v)
{
    return ((t_string *)v.as.obj);
}

int val_is_range(t_value v)
{
    return (v.type == VAL_OBJ && v.as.obj->type == OBJ_RANGE);
}

t_range *val_range(t_value v)
{
    return ((t_range *)v.as.obj);
}

int val_is_tuple(t_value v)
{
    return (v.type == VAL_OBJ && v.as.obj->type == OBJ_TUPLE);
}

t_tuple *val_tuple(t_value v)
{
    return ((t_tuple *)v.as.obj);
}

int val_is_list(t_value v)
{
    return (v.type == VAL_OBJ && v.as.obj->type == OBJ_LIST);
}

t_list  *val_list(t_value v)
{
    return ((t_list *)v.as.obj);
}

int val_is_dict(t_value v)
{
    return (v.type == VAL_OBJ && v.as.obj->type == OBJ_DICT);
}

int val_is_set(t_value v)
{
    return (v.type == VAL_OBJ && v.as.obj->type == OBJ_SET);
}

t_dict  *val_dict(t_value v)
{
    return ((t_dict *)v.as.obj);
}

int val_is_obj(t_value v, t_obj_type type)
{
    return (v.type == VAL_OBJ && v.as.obj->type == type);
}

const char  *val_type_name(t_value v)
{
    if (v.type == VAL_NONE)
        return ("none");
    if (v.type == VAL_BOOL)
        return ("bool");
    if (v.type == VAL_INT || v.type == VAL_FLOAT)
        return ("number");
    if (val_is_range(v))
        return ("range");
    if (val_is_tuple(v))
        return ("tuple");
    if (val_is_list(v))
        return ("list");
    if (val_is_dict(v))
        return ("dict");
    if (val_is_set(v))
        return ("set");
    if (val_is_obj(v, OBJ_FUNCTION) || val_is_obj(v, OBJ_NATIVE)
        || val_is_obj(v, OBJ_BOUND))
        return ("function");
    if (val_is_obj(v, OBJ_CELL))
        return ("cell");
    if (val_is_obj(v, OBJ_ERROR))
        return (error_type_name(((t_error_value *)v.as.obj)->err.type));
    if (val_is_obj(v, OBJ_INSTANCE))
        return (((t_instance *)v.as.obj)->cls->as.obj.cname);
    if (val_is_obj(v, OBJ_CLASS) || val_is_obj(v, OBJ_METHOD))
        return ("function");
    return ("string");
}
