/*
 * The table of every native function and module constant, built from
 * the lists in resolver/natives.h. Shared by the resolver (names,
 * arities, what each module offers) and the runtime (calls, display).
 */

#include <string.h>
#include "resolver/natives.h"

/* Arguments that can be given by name (see NATIVE_LIST). */
static const char *const    g_kw_print[] = {"sep", "end", "file", NULL};
static const char *const    g_kw_eprint[] = {"sep", "end", NULL};
static const char *const    g_kw_dump[] = {"indent", NULL};
static const char *const    g_names_list_dir[] = {"p", NULL};

#define GLOBAL_DEF(name, min, max, repl, names, kw) \
    {NMOD_NONE, #name, #name, min, max, repl, names, kw},
#define MEMBER_DEF(mod, name, min, max, names, kw) \
    {NMOD_##mod, #name, #mod "." #name, min, max, 0, names, kw},
#define MEMBER_COUNT(...) + 1
#define SKIP(mod, name)
#define CONST_DEF(mod, name) {NMOD_##mod, #name},
#define SKIP_FN(mod, name, min, max, names, kw)
#define MODULE_NAME(mod) #mod,

const t_native_def      g_native_defs[] = {
    NATIVE_LIST(GLOBAL_DEF)
    NATIVE_MEMBERS(MEMBER_DEF, SKIP)
};

const int               g_native_count = sizeof(g_native_defs)
    / sizeof(*g_native_defs);

const int               g_native_global_count = 0 NATIVE_LIST(MEMBER_COUNT);

const t_native_const    g_native_consts[] = {
    NATIVE_MEMBERS(SKIP_FN, CONST_DEF)
};

const int               g_native_const_count = sizeof(g_native_consts)
    / sizeof(*g_native_consts);

const char *const       g_native_module_names[] = {
    NATIVE_MODULES(MODULE_NAME)
};

t_native_module native_module_named(const char *name, size_t len)
{
    int i;

    for (i = 0; i < NMOD_COUNT; i++)
        if (strlen(g_native_module_names[i]) == len
            && memcmp(g_native_module_names[i], name, len) == 0)
            return ((t_native_module)i);
    return (NMOD_NONE);
}

int native_param(const char *const *list, const char *name, size_t len)
{
    int i;

    for (i = 0; list && list[i]; i++)
        if (strlen(list[i]) == len && memcmp(list[i], name, len) == 0)
            return (i);
    return (-1);
}

int native_param_count(const char *const *list)
{
    int i;

    i = 0;
    while (list && list[i])
        i++;
    return (i);
}

/*
 * What to add when a native with keyword-only parameters is given too
 * many positional arguments: how to give them by name. Only json.dump
 * can meet it (print and io.eprint take any number of positional ones).
 */
const char  *native_kw_hint(const t_native_def *def)
{
    if (def->module == NMOD_json && strcmp(def->name, "dump") == 0)
        return ("indent goes by name: dump(v, indent = 2)");
    return (NULL);
}
