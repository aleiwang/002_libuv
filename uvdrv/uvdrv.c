#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "uvdrv.h" 


static int traceback (lua_State *L) { 
	const char *msg = lua_tostring(L, 1);
	if (msg)
		luaL_traceback(L, L, msg, 1);
	else {
		lua_pushliteral(L, "(no error message)");
	}
	return 1;
}

static int unique(lua_State *L) {
    lua_newuserdata(L, 0);
    return 1;
}

static int topointer(lua_State *L) {
    const void *p;
    luaL_checkany(L, 1);
    p = lua_topointer(L, 1);
    lua_pushfstring(L, "%p", p);
    return 1;
}

static int uvdrv_version(lua_State *L) { 
    lua_pushstring(L, uv_version_string());
    return 1; 
}

// static int loop_size(lua_State *L) {
//     lua_pushinteger(L, uv_loop_size()); 
//     return 1;
// }

static int default_loop(lua_State *L) {
    uv_loop_t *loop = uv_default_loop();
    setloop(L, loop);
    return 1;
}


static int new_loop(lua_State *L) {
    uv_loop_t *loop = (uv_loop_t *)malloc(sizeof(uv_loop_t)); 
    uv_loop_init(loop); 
    setloop(L, loop);
    return 1;
}

static int loop_destroy(lua_State *L) {
    loop_t *lo = get_loop(L, 1);
    uv_loop_t* loop = lo->loop;
    if(loop) {
        lo->loop = 0;
        uv_loop_close(loop); 
        free(loop); 
    }
    return 0;
}

static int loop_tostring(lua_State *L) {
    loop_t *lo = get_loop(L, 1);
    lua_pushfstring(L, "loop: %p", lo);
    return 1;
}


LOOP_METHOD_VOID(stop)
LOOP_METHOD_INT(loop_alive)
LOOP_METHOD_VOID(update_time)
LOOP_METHOD_INT(now)
LOOP_METHOD_INT(backend_fd)
LOOP_METHOD_INT(backend_timeout)

// int uv_run(uv_loop_t*, uv_run_mode mode); 
// L statck:
//  4: ud
//  3: cb
//  2: flags
//  1: loop
static int loop_run(lua_State *L) {
    uv_loop_t *loop;  
    loop_t *lo = get_loop(L, 1);
    int flags = luaL_checkinteger(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    luaL_checkany(L, 4);
    lua_pushcfunction(L, traceback);

    loop = lo->loop;
    loop->data = L;
    lua_pushinteger(L, uv_run(loop, flags));
    loop->data = NULL; 
    return 1;
}

static const struct luaL_Reg mt_loop[] = {
    {"__gc", loop_destroy},
    {"__tostring", loop_tostring},
    {NULL, NULL}
};

static const struct luaL_Reg methods_loop[] = {
	{"run", loop_run}, 
    {"stop", loop_stop},
    {"alive", loop_loop_alive},
    {"update_time", loop_update_time},
    {"now", loop_now},
    {"backend_fd", loop_backend_fd},
    {"backend_timeout", loop_backend_timeout},

    {NULL, NULL}
};


// implement wather call backs
WATCHER_CB_ARG0(idle)
WATCHER_CB_ARG0(timer)
WATCHER_CB_ARG0(prepare)
WATCHER_CB_ARG0(check)
WATCHER_CB_ARG_N(signal)


// idle methods
WATCHER_COMMON_METHODS(idle)
WATCHER_START(idle) 
static const struct luaL_Reg mt_idle[] = {
    {"__tostring", idle_tostring},
    {NULL, NULL}
};
static const struct luaL_Reg methods_idle[] = {
    WATCHER_METAMETHOD_TABLE(idle),
    {NULL, NULL}
};


// signal methods
WATCHER_COMMON_METHODS(signal)
WATCHER_START_N(signal) 
static const struct luaL_Reg mt_signal[] = {
    {"__tostring", signal_tostring},
    {NULL, NULL}
};
static const struct luaL_Reg methods_signal[] = {
    WATCHER_METAMETHOD_TABLE(signal),
    {NULL, NULL}
};

// prepare methods
WATCHER_COMMON_METHODS(prepare)
WATCHER_START(prepare) 
static const struct luaL_Reg mt_prepare[] = {
    {"__tostring", prepare_tostring},
    {NULL, NULL}
};
static const struct luaL_Reg methods_prepare[] = {
    WATCHER_METAMETHOD_TABLE(prepare),
    {NULL, NULL}
};

// check methods
WATCHER_COMMON_METHODS(check)
WATCHER_START(check) 
static const struct luaL_Reg mt_check[] = {
    {"__tostring", check_tostring},
    {NULL, NULL}
};
static const struct luaL_Reg methods_check[] = {
    WATCHER_METAMETHOD_TABLE(check),
    {NULL, NULL}
};

// timer methods 
WATCHER_COMMON_METHODS(timer)
WATCHER_START_LL(timer) 
static const struct luaL_Reg mt_timer[] = {
    {"__tostring", timer_tostring},
    {NULL, NULL}
};
static const struct luaL_Reg methods_timer[] = {
    WATCHER_METAMETHOD_TABLE(timer),
    {NULL, NULL}
};


// create_metatable_*
METATABLE_BUILDER(loop, LOOP_METATABLE)
METATABLE_BUILDER(timer, WATCHER_METATABLE(timer))
METATABLE_BUILDER(signal, WATCHER_METATABLE(signal))
METATABLE_BUILDER(prepare, WATCHER_METATABLE(prepare))
METATABLE_BUILDER(check, WATCHER_METATABLE(check))
METATABLE_BUILDER(idle, WATCHER_METATABLE(idle))


static const struct luaL_Reg uv_module_methods[] = {
    {"version", uvdrv_version},
    {"unique", unique},
    {"topointer", topointer},    
    {"default_loop", default_loop},
    {"new_loop", new_loop},
    {"new_timer", new_timer},
    {"new_signal", new_signal},
    {"new_prepare", new_prepare},
    {"new_check", new_check},
    {"new_idle", new_idle},
    {NULL, NULL}
};


LUALIB_API int luaopen_uvdrv_c(lua_State *L) {
    luaL_checkversion(L);

    // call create metatable
    CREATE_METATABLE(loop, L);
    CREATE_METATABLE(timer, L);
    CREATE_METATABLE(signal, L);
    CREATE_METATABLE(prepare, L);
    CREATE_METATABLE(check, L);
    CREATE_METATABLE(idle, L);

    luaL_newlib(L, uv_module_methods);

    // add constant
    ADD_CONSTANT(L, UV_RUN_DEFAULT);
    ADD_CONSTANT(L, UV_RUN_ONCE);
    ADD_CONSTANT(L, UV_RUN_NOWAIT);

    // add signal 
    ADD_CONSTANT(L, SIGHUP);
    ADD_CONSTANT(L, SIGINT);
    ADD_CONSTANT(L, SIGTERM);
    #ifdef SIGPIPE
    ADD_CONSTANT(L, SIGPIPE);
    #endif 
    
    return 1;
}
