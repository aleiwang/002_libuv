#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "uv.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#ifdef __cplusplus	
}
#endif 

#include <stdint.h>
#include <stddef.h>

/* for compatible with lua5.1 */
#ifndef LUA_OK
#define LUA_OK 0
#endif


#ifdef _WIN32
#define INLINE __inline
#else
#define INLINE inline
#endif

typedef struct loop_t {
    uv_loop_t *loop;
} loop_t;

typedef struct {
     uv_write_t req;
     uv_buf_t   buf;
} write_req_t;

INLINE static void _add_unsigned_constant(lua_State *L, const char* name, unsigned int value) {
    lua_pushinteger(L, value);
    lua_setfield(L, -2, name);
}


#define ADD_CONSTANT(L, name) _add_unsigned_constant(L, #name, name);

/**
========================defines========================
*/
#define LOG printf
#define LOOP_METATABLE "loop_metatable"
#define WATCHER_METATABLE(type) "watcher_" #type "_metatable"
#define METATABLE_BUILDER_NAME(type) create_metatable_##type
#define METATABLE_BUILDER(type, name) \
    static void METATABLE_BUILDER_NAME(type) (lua_State *L) { \
        if(luaL_newmetatable(L, name)) { \
            luaL_setfuncs(L, mt_##type, 0); \
            luaL_newlib(L, methods_##type); \
            lua_setfield(L, -2, "__index"); \
        }\
        lua_pop(L, 1); \
    }

#define CREATE_METATABLE(type, L) METATABLE_BUILDER_NAME(type)(L)


INLINE static loop_t* get_loop(lua_State *L, int index) {
    loop_t *lo = (loop_t*)luaL_checkudata(L, index, LOOP_METATABLE);
    return lo;
}

INLINE static void setloop(lua_State *L, uv_loop_t *loop) {
    loop_t *lo = (loop_t*)lua_newuserdata(L, sizeof(loop_t));
    luaL_getmetatable(L, LOOP_METATABLE);
    lua_setmetatable(L, -2);

    lo->loop = loop;
}

// *L = uv_userdata(loop)
// L statck:
//  -1: trace
//  -2: ud
//  -3: lua function
#define WATCHER_CB_ARG0(type) \
    static void watcher_cb_##type(uv_##type##_t * handle) { \
	    lua_State * L =  (lua_State *)(handle->loop->data); \
	    int traceback = lua_gettop(L);\
	    int r;\
	    assert(L != NULL);\
	    assert(lua_isfunction(L, traceback));\
	    assert(lua_isfunction(L, traceback - 2));\
	    lua_pushvalue(L, traceback - 2); \
	    lua_pushvalue(L, traceback - 1); \
	    lua_pushlightuserdata(L, (void*)handle);\
	    r = lua_pcall(L, 2, 0, traceback);\
	    if(r == LUA_OK) {\
	        return;\
	    }\
	    LOG("watcher(%p) callback failed, errcode:%d, msg: %s", handle, r, lua_tostring(L, -1));\
	    lua_pop(L, 1);\
	    return;\
    }

#define WATCHER_CB_ARG_N(type) \
    static void watcher_cb_##type(uv_##type##_t * handle, int n) { \
        lua_State * L =  (lua_State *)(handle->loop->data); \
        int traceback = lua_gettop(L);\
        int r;\
        assert(L != NULL);\
        assert(lua_isfunction(L, traceback));\
        assert(lua_isfunction(L, traceback - 2));\
        lua_pushvalue(L, traceback - 2); \
        lua_pushvalue(L, traceback - 1); \
        lua_pushlightuserdata(L, (void*)handle);\
        lua_pushinteger(L, n);\
        r = lua_pcall(L, 3, 0, traceback);\
        if(r == LUA_OK) {\
            return;\
        }\
        LOG("watcher(%p) callback failed, errcode:%d, msg: %s", handle, r, lua_tostring(L, -1));\
        lua_pop(L, 1);\
        return;\
    }


// void uv_*(loop)
#define LOOP_METHOD_VOID(name) \
    static int loop_##name(lua_State *L) { \
        loop_t *lo = get_loop(L, 1); \
        uv_##name(lo->loop); \
        return 0; \
    }

// int/unsigned/boolean/number uv_*(loop)
#define LOOP_METHOD_RET(name, type) \
    static int loop_##name(lua_State *L) { \
        loop_t *lo = get_loop(L, 1); \
        lua_push##type(L, uv_##name(lo->loop)); \
        return 1; \
    }

// void uv_*(loop, arg1)
#define LOOP_METHOD_VOID_ARG_ARG(name, carg) \
    static int loop_##name(lua_State *L) { \
        loop_t *lo = get_loop(L, 1); \
        carg a1 = luaL_checkinteger(L, 2); \
        uv_##name(lo->loop, a1); \
        return 0; \
    }

// int/unsigned/boolean/number uv_*(loop, arg1)
#define LOOP_METHOD_RET_ARG_ARG(name, type, carg) \
    static int loop_##name(lua_State *L) { \
        loop_t *lo = get_loop(L, 1); \
        carg a1 = luaL_checkinteger(L, 2); \
        lua_push##type(L, uv_##name(lo->loop, a1)); \
        return 1; \
    }


#define LOOP_METHOD_BOOL(name) LOOP_METHOD_RET(name, boolean)
#define LOOP_METHOD_INT(name) LOOP_METHOD_RET(name, integer)
#define LOOP_METHOD_UNSIGNED(name) LOOP_METHOD_RET(name, integer)
#define LOOP_METHOD_DOUBLE(name) LOOP_METHOD_RET(name, number)
#define LOOP_METHOD_VOID_INT(name) LOOP_METHOD_VOID_ARG_ARG(name, int)
#define LOOP_METHOD_BOOL_INT(name) LOOP_METHOD_RET_ARG_ARG(name, boolean, int)


// watcher
#define WATCHER_NEW(type) \
    static int new_##type(lua_State *L) { \
    	loop_t *lo = get_loop(L, 1); \
        uv_##type##_t *w = (uv_##type##_t*)lua_newuserdata(L, sizeof(*w)); \
        uv_##type##_init(lo->loop, w);\
        luaL_getmetatable(L, WATCHER_METATABLE(type)); \
        lua_setmetatable(L, -2); \
        return 1;\
    }

#define WATCHER_GET(type) \
    INLINE static uv_##type##_t* get_##type(lua_State *L, int index) { \
        uv_##type##_t *w = (uv_##type##_t*)luaL_checkudata(L, index, WATCHER_METATABLE(type)); \
        return w; \
    }

#define WATCHER_TOSTRING(type) \
    static int type##_tostring(lua_State *L) { \
        uv_##type##_t *w = get_##type(L, 1); \
        lua_pushfstring(L, "%s: %p", #type, w); \
        return 1; \
    }

#define WATCHER_ID(type) \
    static int type##_id(lua_State *L) { \
        uv_##type##_t *w = get_##type(L, 1); \
        lua_pushlightuserdata(L, (void*)w); \
        return 1; \
    }


#define WATCHER_START(type) \
    static int type##_start(lua_State *L) { \
        uv_##type##_t *w = get_##type(L, 1); \
        uv_##type##_start(w, watcher_cb_##type); \
        return 0; \
    }

#define WATCHER_START_N(type) \
    static int type##_start(lua_State *L) { \
        uv_##type##_t *w = get_##type(L, 1); \
        int a2 = luaL_checkinteger(L, 2); \
        uv_##type##_start(w, watcher_cb_##type, a2); \
        return 0; \
    }

#define WATCHER_START_LL(type) \
    static int type##_start(lua_State *L) { \
        uv_##type##_t *w = get_##type(L, 1); \
        uint64_t a2 = luaL_checkinteger(L, 2); \
        uint64_t a3 = luaL_checkinteger(L, 3); \
        uv_##type##_start(w, watcher_cb_##type, a2, a3); \
        return 0; \
    }

#define WATCHER_STOP(type) \
    static int type##_stop(lua_State *L) { \
        uv_##type##_t *w = get_##type(L, 1); \
        uv_##type##_stop(w); \
        return 0; \
    }

#define WATCHER_IS_ACTIVE(type) \
    static int type##_is_active(lua_State *L) { \
        uv_##type##_t *w = get_##type(L, 1); \
        lua_pushboolean(L, uv_is_active((const uv_handle_t*)w)); \
        return 1; \
    }


#define WATCHER_COMMON_METHODS(type) \
    WATCHER_NEW(type) \
    WATCHER_GET(type) \
    WATCHER_TOSTRING(type) \
    WATCHER_ID(type) \
    WATCHER_STOP(type) \
    WATCHER_IS_ACTIVE(type) \

#define WATCHER_METAMETHOD_ITEM(type, name) {#name, type##_##name}

#define WATCHER_METAMETHOD_TABLE(type) \
    WATCHER_METAMETHOD_ITEM(type, id), \
    WATCHER_METAMETHOD_ITEM(type, start), \
    WATCHER_METAMETHOD_ITEM(type, stop), \
    WATCHER_METAMETHOD_ITEM(type, is_active) 

#define WATCHER_COMMON_METHODS_TCP(type) \
    WATCHER_NEW(type) \
    WATCHER_GET(type) \
    WATCHER_TOSTRING(type) \
    WATCHER_ID(type) \
    WATCHER_IS_ACTIVE(type) 

#define WATCHER_METAMETHOD_TABLE_TCP(type) \
    WATCHER_METAMETHOD_ITEM(type, id), \
    WATCHER_METAMETHOD_ITEM(type, is_active)     

