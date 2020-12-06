#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "uvdrv.h" 
#include "ringbuf.h"
#include <map> 

enum TCP_CALLBACK {
    TCP_LISTEN_CB,
    TCP_READ_CB,
    TCP_CLOSED_CB,
    TCP_CONNECT_RET_CB,
}; 

typedef std::pair<void*, ringbuffer_t> MEMPAIR;
typedef std::map<void*, ringbuffer_t> MEMMAP;
static MEMMAP s_mem_map;
static uint8_t * alloc_ring_buf(ringbuffer_t * ringBuf) {
    // 5k 
    uint32_t len = 1024*5;
    uint8_t * pMem = (uint8_t *)malloc(len); 
    create_ringBuffer(ringBuf, pMem, len); 
    return pMem;
}

static void free_ring_buf(ringbuffer_t * ringBuf) {
    if (ringBuf->source)
        free( ringBuf->source ); 
    ringBuf->source = NULL; 
}

static inline uint32_t read_size(uint8_t* buffer) {
    uint32_t r = (int)buffer[0] << 8 | (int)buffer[1];
    return r;
}

static inline void write_size(uint8_t* buffer, int len) {
    buffer[0] = (len >> 8) & 0xff;
    buffer[1] = len & 0xff;
}

void free_write_req(uv_write_t *req) {
    write_req_t *wr = (write_req_t*) req;
    free(wr->buf.base);
    free(wr);
}

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = (char*) malloc(suggested_size); 
    buf->len = suggested_size;
}

void echo_write(uv_write_t *req, int status) {
    if (status) {
        fprintf(stderr, "Write error %s\n", uv_strerror(status));
    }
    free_write_req(req);
}

void tcp_connect_cb(uv_connect_t* req, int status){
    uv_handle_t * handle = (uv_handle_t *)req->data;  
    free(req); 

    lua_State * L =  (lua_State *)(handle->loop->data); 
    int traceback = lua_gettop(L);
    int r;
    assert(L != NULL);
    assert(lua_isfunction(L, traceback));
    assert(lua_isfunction(L, traceback - 2));
    lua_pushvalue(L, traceback - 2); 
    lua_pushvalue(L, traceback - 1); 
    lua_pushlightuserdata(L, (void*)handle);
    lua_pushinteger(L, TCP_CONNECT_RET_CB);
    lua_pushinteger(L, status);
    r = lua_pcall(L, 4, 0, traceback);
    if(r == LUA_OK) {
        return;
    }
    LOG("tcp(%p) tcp_connect_cb failed, errcode:%d, msg: %s", handle, r, lua_tostring(L, -1));
    lua_pop(L, 1);
    return;        
}

void tcp_close_cb(uv_handle_t* handle) {
    void * pkey = (void*)handle; 
    MEMMAP::iterator it = s_mem_map.find(pkey);
    if (it != s_mem_map.end()) { 
        free_ring_buf(&it->second);
        s_mem_map.erase(it);
    }

    lua_State * L =  (lua_State *)(handle->loop->data); 
    int traceback = lua_gettop(L);
    int r;
    assert(L != NULL);
    assert(lua_isfunction(L, traceback));
    assert(lua_isfunction(L, traceback - 2));
    lua_pushvalue(L, traceback - 2); 
    lua_pushvalue(L, traceback - 1); 
    lua_pushlightuserdata(L, (void*)handle);
    lua_pushinteger(L, TCP_CLOSED_CB);
    r = lua_pcall(L, 3, 0, traceback);
    if(r == LUA_OK) {
        return;
    }
    LOG("tcp(%p) tcp_close_cb failed, errcode:%d, msg: %s", handle, r, lua_tostring(L, -1));
    lua_pop(L, 1);
    return;    
}

void echo_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) { 
    if (nread > 0) {

        void * pkey = (void*)client;
        MEMMAP::iterator it = s_mem_map.find(pkey);
        if (it == s_mem_map.end()) {
            ringbuffer_t ring;
            s_mem_map.insert(MEMPAIR(pkey, ring));
            it = s_mem_map.find(pkey); 
            if (it != s_mem_map.end()) {
                ringbuffer_t * pRing = &(it->second); 
                alloc_ring_buf(pRing); 
            }            
        }

        if (it != s_mem_map.end()) {
            ringbuffer_t * pRing = &(it->second); 
            if (get_ringBuffer_bcanWrite(pRing) < nread) {
                fprintf(stderr, "mem too big to write %d\n", (int)nread);  
            } else {
                write_ringBuffer((uint8_t*)(buf->base), nread, pRing); 
                uint32_t buf_size_canread = get_ringBuffer_btoRead(pRing); 

                while (buf_size_canread > 2) { 
                    uint8_t header[2] = {0}; 
                    if (peek_ringBuffer(header, 2, pRing) == 2) {
                        uint32_t bodySize = read_size(header); 
                        if (bodySize <= (buf_size_canread - 2))  {
                            read_ringBuffer(header, 2, pRing); 

                            uint8_t body[bodySize]; 
                            read_ringBuffer(body, bodySize, pRing);

                            /**
                            throw pack to lua 
                            */
                            lua_State * L =  (lua_State *)(client->loop->data); 
                            int traceback = lua_gettop(L);
                            int r;
                            assert(L != NULL);
                            assert(lua_isfunction(L, traceback));
                            assert(lua_isfunction(L, traceback - 2));
                            lua_pushvalue(L, traceback - 2); 
                            lua_pushvalue(L, traceback - 1); 
                            lua_pushlightuserdata(L, (void*)client);
                            lua_pushinteger(L, TCP_READ_CB);
                            lua_pushlstring(L, (const char *)body, bodySize);
                            r = lua_pcall(L, 4, 0, traceback);

                            if(r != LUA_OK) {
                                LOG("tcp(%p) echo_read failed, errcode:%d, msg: %s", client, r, lua_tostring(L, -1));
                                lua_pop(L, 1);
                            }

                            buf_size_canread = get_ringBuffer_btoRead(pRing); 
                        }
                    }
                }
            }
        }
    }
    else if (nread < 0) { 
        if (nread != UV_EOF) 
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t*) client, tcp_close_cb); 
    }

    free(buf->base); 
}

static struct sockaddr * get_sock_addr(const char* ip, int port, struct sockaddr_in * ipv4, struct sockaddr_in6 * ipv6) {
    struct sockaddr * r = NULL;

    if( strstr( ip, ":" ) ) {
        uv_ip6_addr( ip, port, ipv6 );
        r = (struct sockaddr*)ipv6;
    }
    else {
        uv_ip4_addr(ip, port, ipv4);
        r = (struct sockaddr*)ipv4;
    }

    return r;
}

WATCHER_COMMON_METHODS_TCP(tcp)

static void tcp_cb_listen(uv_stream_t * handle, int status) {  
    lua_State * L =  (lua_State *)(handle->loop->data); 
    int traceback = lua_gettop(L);
    int r;
    assert(L != NULL);
    assert(lua_isfunction(L, traceback));
    assert(lua_isfunction(L, traceback - 2));
    lua_pushvalue(L, traceback - 2); 
    lua_pushvalue(L, traceback - 1); 
    lua_pushlightuserdata(L, (void*)handle);
    lua_pushinteger(L, TCP_LISTEN_CB);
    lua_pushinteger(L, status);
    r = lua_pcall(L, 4, 0, traceback);
    if(r == LUA_OK) {
        LOG("tcp(%p) tcp_cb_listen sucess, errcode:%d", handle, r);
        return;
    }
    LOG("tcp(%p) tcp_cb_listen failed, errcode:%d, msg: %s", handle, r, lua_tostring(L, -1));
    lua_pop(L, 1);
    return;
}

static int tcp_bind(lua_State *L) { 
    uv_tcp_t *w = get_tcp(L, 1); 
    const char * str_add = luaL_checkstring(L, 2); 
    int port = luaL_checkinteger(L, 3); 

    struct sockaddr* realAddr = NULL;
    struct sockaddr_in addr4;
    struct sockaddr_in6 addr6;
    realAddr = get_sock_addr( str_add, port, &addr4, &addr6 );    
    int nret = uv_tcp_bind(w,(const struct sockaddr*)realAddr,0); 

    lua_pushinteger(L, nret);
    return 1; 
}

static int tcp_listen(lua_State *L) {  
    uv_tcp_t *w = get_tcp(L, 1); 
    int backlen = luaL_checkinteger(L, 2); 
    int nret = uv_listen((uv_stream_t *)w,backlen,tcp_cb_listen); 
    lua_pushinteger(L, nret);
    return 1; 
}

static int tcp_accept(lua_State *L) {  
    uv_tcp_t * acceptor        = get_tcp(L, 1); 
    uv_tcp_t * accepted_client = get_tcp(L, 2); 

    int nret = uv_accept((uv_stream_t *)acceptor, (uv_stream_t *)accepted_client); 
    lua_pushinteger(L, nret);
    return 1; 
}

static int tcp_read(lua_State *L) {  
    uv_tcp_t * client = get_tcp(L, 1); 
    int nret = uv_read_start((uv_stream_t *)client, alloc_buffer, echo_read); 
    lua_pushinteger(L, nret);
    return 1; 
}

static int tcp_close(lua_State *L) {  
    uv_tcp_t * client = get_tcp(L, 1); 
    uv_close((uv_handle_t*) client, tcp_close_cb);
    return 0; 
}

static int tcp_write(lua_State *L) {  
    size_t len = 0;
    uv_tcp_t * client = get_tcp(L, 1); 
    const char * str = lua_tolstring(L, 2, &len); 
    assert (len <= 65535);

    write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
    uint8_t * header = (uint8_t *)malloc(len + 2);  

    write_size(header, len); 
    memcpy(header + 2, str, len); 

    req->buf = uv_buf_init((char *)header, len + 2); 
    int nret = uv_write((uv_write_t*) req, (uv_stream_t *)client, &(req->buf), 1, echo_write); 

    lua_pushinteger(L, nret);
    return 1;     
}

static int tcp_connect(lua_State *L) { 
    uv_tcp_t * client = get_tcp(L, 1); 
    const char * str_add = luaL_checkstring(L, 2); 
    int port = luaL_checkinteger(L, 3);     
    uv_connect_t * req = (uv_connect_t *)malloc(sizeof(uv_connect_t));  
    req->data = (void *)client;

    struct sockaddr* realAddr = NULL;
    struct sockaddr_in addr4;
    struct sockaddr_in6 addr6;
    realAddr = get_sock_addr( str_add, port, &addr4, &addr6 );     

    int nret = uv_tcp_connect( req, client, (const struct sockaddr*) realAddr, tcp_connect_cb ); 
    lua_pushinteger(L, nret);
    return 1;     
}

static const struct luaL_Reg mt_tcp[] = {
    {"__tostring", tcp_tostring},
    {NULL, NULL}
};

static const struct luaL_Reg methods_tcp[] = {
    WATCHER_METAMETHOD_TABLE_TCP(tcp),
    {"bind",    tcp_bind}, 
    {"listen",  tcp_listen}, 
    {"accept",  tcp_accept}, 
    {"read",    tcp_read}, 
    {"write",   tcp_write}, 
    {"close",   tcp_close}, 
    {"connect", tcp_connect}, 
    {NULL, NULL}
};


METATABLE_BUILDER(tcp, WATCHER_METATABLE(tcp))
static const struct luaL_Reg uv_module_tcp[] = {
    {"new_tcp", new_tcp}, 
    {NULL, NULL}
};

extern "C" {
    LUALIB_API int luaopen_uvdrv_tcp(lua_State *L) {
        luaL_checkversion(L);
        CREATE_METATABLE(tcp, L);
        luaL_newlib(L, uv_module_tcp);

        // add constant
        ADD_CONSTANT(L, TCP_LISTEN_CB);
        ADD_CONSTANT(L, TCP_READ_CB);
        ADD_CONSTANT(L, TCP_CLOSED_CB);
        ADD_CONSTANT(L, TCP_CONNECT_RET_CB);

        return 1;
    }
}

