#include <flecs_lua.h>

#include <lualib.h>
#include <lauxlib.h>

ECS_STRUCT(lua_test_comp,
{
    float foo;
    uint16_t u16a[4];
});

ECS_STRUCT(lua_test_comp2,
{
    lua_test_comp comp;
    int32_t bar;
});

ECS_ENUM(lua_test_enum,
{
    TestEnum1,
    TestEnum2
});

ECS_BITMASK(lua_test_bitmask,
{
    One = 1,
    Two = 2,
    Three = 3
});

ECS_VECTOR(lua_test_vector,float);

ECS_MAP(lua_test_mapi32, int32_t, float);

ECS_STRUCT(lua_test_struct,
{
    bool b;
    char c;
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    float f32;
    double f64;

    char ca[4];

    char *str;
    void *vptr;
    uintptr_t uptr;

    lua_test_enum enumeration;
    lua_test_bitmask bitmask;
    lua_test_vector vector;

    lua_test_comp comp;
    lua_test_comp2 comp2;
});

ECS_DECLARE_COMPONENT(lua_test_comp);
ECS_DECLARE_COMPONENT(lua_test_comp2);
ECS_DECLARE_COMPONENT(lua_test_struct);

struct vars
{
    lua_test_struct s;
};

static struct vars g;
static struct vars g_out;

#define TEST_COMP_INIT { .foo = 123.4f, .u16a = 123, 4321, 32, 688}

static void init_globals(void)
{
    lua_test_struct s =
    {
        .b = true,
        .c = 1,
        .u8 = 2,
        .u16 = 4,
        .u32 = 8,
        .u64 = 16,
        .i8 = 32,
        .i16 = 64,
        .i32 = 128,
        .i64 = 256,
        .f32 = 512,
        .f64 = 1024,

        .ca = { 10, 20, 30, 40 },

        .str = "test string",
        .vptr = &g,
        .uptr = (uintptr_t)&g,

        .enumeration = 465,
        .bitmask = One | Two,

        .comp.foo = 4.0f,
        .comp.u16a = { 10, 20, 30, 40 },
        .comp2.bar = 5,
        .comp2.comp.u16a = { 100, 200, 300, 400 }
    };

    memcpy(&g.s, &s, sizeof(s));
}

int lpush_test_struct(lua_State *L)
{
    ecs_world_t *w = ecs_lua_get_world(L);

    ecs_entity_t e = ecs_lookup_fullpath(w, "lua_test_struct");
    ecs_assert(e, ECS_INTERNAL_ERROR, NULL);

    ecs_ptr_to_lua(w, L, e, &g.s);

    return 1;
}

int lset_test_struct(lua_State *L)
{
    ecs_world_t *w = ecs_lua_get_world(L);

    return 1;
}

static const luaL_Reg test_lib[] =
{
    { "struct", lpush_test_struct },
    { "set_struct", lset_test_struct },
    { NULL, NULL }
};

int luaopen_test(lua_State *L)
{
    luaL_newlib(L, test_lib);
    return 1;
}

static void init_test_state(lua_State *L)
{
    luaL_openlibs(L);

    luaL_requiref(L, "test", luaopen_test, 0);
    lua_pop(L, 1);
}

static int custom_alloc;
static size_t mem_usage;

void *Allocf(void *ud, void *ptr, size_t osize, size_t nsize)
{
    custom_alloc = 1;

    if(!ptr) osize = 0;

    if(!nsize)
    {
        mem_usage -= osize;
        ecs_os_free(ptr);
        return NULL;
    }

    mem_usage += (nsize - osize);
    return ecs_os_realloc(ptr, nsize);
}

static lua_State *new_test_state(void)
{
    lua_State *L = lua_newstate(Allocf, NULL);

    init_test_state(L);

    return L;
}

static void test_abort(void)
{
    printf("TEST: ecs_os_abort() was called!\n");
    fflush(stdout);
    printf("\n");
}

static void test_msg(const char *type, const char *fmt, va_list args)
{
    printf("TEST %s: ", type);
    vprintf(fmt, args);
    printf("\n");
}

static void test_log(const char *fmt, va_list args)
{
    test_msg("LOG", fmt, args);
}

static void test_error(const char *fmt, va_list args)
{
    test_msg("ERROR", fmt, args);
}

static void test_debug(const char *fmt, va_list args)
{
    test_msg("DBG", fmt, args);
}

static void test_warn(const char *fmt, va_list args)
{
    test_msg("WARN", fmt, args);
}

ECS_CTOR(lua_test_struct, ptr,
{
    ptr->str = NULL;
});

ECS_DTOR(lua_test_struct, ptr,
{
    ecs_os_free(ptr->str);
    ptr->str = NULL;
});

ECS_COPY(lua_test_struct, dst, src,
{
    if(dst->str)
    {
        ecs_os_free(dst->str);
        dst->str = NULL;
    }

    if(src->str) dst->str = ecs_os_strdup(src->str);
    else dst->str = NULL;

    void *t = dst->str;
    memcpy(dst, src, sizeof(lua_test_struct));
    dst->str = t;
});

ECS_MOVE(lua_test_struct, dst, src,
{
    ecs_os_free(dst->str);

    memcpy(dst, src, sizeof(lua_test_struct));

    src->str = NULL;
});

int main(int argc, char **argv)
{
    if(argc < 2) return 1;

    init_globals();

    ecs_os_set_api_defaults();
    ecs_os_api_t os_api = ecs_os_api;
    os_api.abort_ = test_abort;
    /*os_api.log_ = test_log;
    os_api.log_error_ = test_error;
    os_api.log_debug_ = test_debug;
    os_api.log_warning_ = test_warn;*/
    ecs_os_set_api(&os_api);

    ecs_world_t *w = ecs_init();

    ECS_IMPORT(w, FlecsLua);

    ECS_META(w, lua_test_comp);
    ECS_META(w, lua_test_comp2);
    ECS_META(w, lua_test_enum);
    ECS_META(w, lua_test_bitmask);
    ECS_META(w, lua_test_vector);
    ECS_META(w, lua_test_mapi32);
    ECS_META(w, lua_test_struct);


    ecs_set_component_actions(w, lua_test_struct,
    {
        .ctor = ecs_ctor(lua_test_struct),
        .dtor = ecs_dtor(lua_test_struct),
        .copy = ecs_copy(lua_test_struct),
        .move = ecs_move(lua_test_struct)
    });


    ecs_new_entity(w, 8192, "ecs_lua_test_c_ent", NULL);
    ecs_set(w, 0, lua_test_comp, TEST_COMP_INIT);
    ecs_set(w, 0, lua_test_comp, TEST_COMP_INIT);
    ecs_set(w, 0, lua_test_comp, TEST_COMP_INIT);

    ecs_set_ptr(w, EcsSingleton, lua_test_struct, &g.s);
    ecs_set_ptr(w, ecs_typeid(lua_test_struct), lua_test_struct, &g.s);

    //const EcsLuaHost *host = ecs_singleton_get(w, EcsLuaHost);
    //lua_State *L = host->L;

    lua_State *L = new_test_state();
    ecs_lua_set_state(w, L);
    ecs_assert(custom_alloc, ECS_INTERNAL_ERROR, NULL);

    init_test_state(L);

    int ret = luaL_dofile(L, argv[1]);

    if(ret)
    {
        const char *err = lua_tostring(L, 1);
        ecs_os_err("script error: %s\n", err);
    }

    int runs = 2;

    while(ecs_progress(w, 0) && runs)
    {
        ecs_lua_progress(L);

        runs--;
    }

    ecs_fini(w);

    return ret;
}