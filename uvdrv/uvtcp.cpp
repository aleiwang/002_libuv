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
    TCP_WRITE_CB,
}; 

enum UDP_CALLBACK {
    UDP_READ_CB,
    UDP_CLOSED_CB,
    UDP_WRITE_CB,
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

size_t free_write_req(uv_write_t *req) {
    write_req_t *wr = (write_req_t*) req;
    size_t len = wr->buf.len;
    free(wr->buf.base);
    free(wr);
    return len;
}

size_t free_write_req_udp(uv_udp_send_t *req) {
    udp_send_req_t *wr = (udp_send_req_t*) req;
    size_t len = wr->buf.len;
    free(wr->buf.base);
    free(wr);
    return len;
}

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = (char*) malloc(suggested_size); 
    buf->len = suggested_size;
}

void echo_write_udp(uv_udp_send_t *req, int status) {
    if (status) {
        fprintf(stderr, "udp send error %s\n", uv_strerror(status));
    }

    uv_handle_t * handle = (uv_handle_t *)req->data;  
    size_t len = free_write_req_udp(req);

    lua_State * L =  (lua_State *)(handle->loop->data); 
    int traceback = lua_gettop(L);
    int r;
    assert(L != NULL);
    assert(lua_isfunction(L, traceback));
    assert(lua_isfunction(L, traceback - 2));
    lua_pushvalue(L, traceback - 2); 
    lua_pushvalue(L, traceback - 1); 
    lua_pushfstring(L, "%p_%d", (void*)handle, UDP_WRITE_CB); 
    lua_pushinteger(L, status);
    lua_pushinteger(L, len);
    r = lua_pcall(L, 4, 0, traceback);
    if(r == LUA_OK) {
        return;
    }
    LOG("udp(%p) echo_write_udp failed, errcode:%d, msg: %s", handle, r, lua_tostring(L, -1));
    lua_pop(L, 1);
    return;       
}

void echo_write(uv_write_t *req, int status) {
    if (status) {
        fprintf(stderr, "Write error %s\n", uv_strerror(status));
    }

    uv_handle_t * handle = (uv_handle_t *)req->data;  
    size_t len = free_write_req(req);

    lua_State * L =  (lua_State *)(handle->loop->data); 
    int traceback = lua_gettop(L);
    int r;
    assert(L != NULL);
    assert(lua_isfunction(L, traceback));
    assert(lua_isfunction(L, traceback - 2));
    lua_pushvalue(L, traceback - 2); 
    lua_pushvalue(L, traceback - 1); 
    lua_pushfstring(L, "%p_%d", (void*)handle, TCP_WRITE_CB); 
    lua_pushinteger(L, status);
    lua_pushinteger(L, len);
    r = lua_pcall(L, 4, 0, traceback);
    if(r == LUA_OK) {
        return;
    }
    LOG("tcp(%p) echo_write failed, errcode:%d, msg: %s", handle, r, lua_tostring(L, -1));
    lua_pop(L, 1);
    return;   
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
    lua_pushfstring(L, "%p_%d", (void*)handle, TCP_CONNECT_RET_CB); 
    lua_pushinteger(L, status);
    r = lua_pcall(L, 3, 0, traceback);
    if(r == LUA_OK) {
        return;
    }
    LOG("tcp(%p) tcp_connect_cb failed, errcode:%d, msg: %s", handle, r, lua_tostring(L, -1));
    lua_pop(L, 1);
    return;        
}

void udp_close_cb(uv_handle_t* handle) {
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
    lua_pushfstring(L, "%p_%d", (void*)handle, UDP_CLOSED_CB); 
    r = lua_pcall(L, 2, 0, traceback);
    if(r == LUA_OK) {
        return;
    }
    LOG("udp(%p) udp_close_cb failed, errcode:%d, msg: %s", handle, r, lua_tostring(L, -1));
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
    lua_pushfstring(L, "%p_%d", (void*)handle, TCP_CLOSED_CB); 
    r = lua_pcall(L, 2, 0, traceback);
    if(r == LUA_OK) {
        return;
    }
    LOG("tcp(%p) tcp_close_cb failed, errcode:%d, msg: %s", handle, r, lua_tostring(L, -1));
    lua_pop(L, 1);
    return;    
}

void echo_read_udp(uv_udp_t* client, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags) { 
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

        char ip_addr[128];
        uv_ip4_name((struct sockaddr_in*)addr, ip_addr, 128);
        int port = ntohs(((struct sockaddr_in*)addr)->sin_port);
        printf("udp recv ip: %s:%d nread = %d\n", ip_addr, port, nread);

        if (it != s_mem_map.end()) {
            ringbuffer_t * pRing = &(it->second); 
            if (get_ringBuffer_bcanWrite(pRing) < nread) {
                fprintf(stderr, "mem too big to write %d\n", (int)nread);  
            } else {
                write_ringBuffer((uint8_t*)(buf->base), nread, pRing); 
                uint32_t buf_size_canread = get_ringBuffer_btoRead(pRing); 
                uint8_t body[buf_size_canread]; 
                read_ringBuffer(body, buf_size_canread, pRing);
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
                lua_pushfstring(L, "%p_%d", (void*)client, UDP_READ_CB);
                lua_pushlstring(L, (const char *)body, buf_size_canread);
                r = lua_pcall(L, 3, 0, traceback);

                if(r != LUA_OK) {
                    LOG("udp(%p) echo_read_udp failed, errcode:%d, msg: %s", client, r, lua_tostring(L, -1));
                    lua_pop(L, 1);
                }
            }
        }
    }
    else if (nread < 0) { 
        if (nread != UV_EOF) 
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t*) client, udp_close_cb); 
    }

    free(buf->base);     
}

void echo_read_nohead(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) { 
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
                uint8_t body[buf_size_canread]; 
                read_ringBuffer(body, buf_size_canread, pRing);
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
                lua_pushfstring(L, "%p_%d", (void*)client, TCP_READ_CB);
                lua_pushlstring(L, (const char *)body, buf_size_canread);
                r = lua_pcall(L, 3, 0, traceback);

                if(r != LUA_OK) {
                    LOG("tcp(%p) echo_read failed, errcode:%d, msg: %s", client, r, lua_tostring(L, -1));
                    lua_pop(L, 1);
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
                            lua_pushfstring(L, "%p_%d", (void*)client, TCP_READ_CB);
                            lua_pushlstring(L, (const char *)body, bodySize);
                            r = lua_pcall(L, 3, 0, traceback);

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

WATCHER_COMMON_METHODS_SOCKET(tcp)
WATCHER_COMMON_METHODS_SOCKET(udp)

static void tcp_cb_listen(uv_stream_t * handle, int status) {  
    lua_State * L =  (lua_State *)(handle->loop->data); 
    int traceback = lua_gettop(L);
    int r;
    assert(L != NULL);
    assert(lua_isfunction(L, traceback));
    assert(lua_isfunction(L, traceback - 2));
    lua_pushvalue(L, traceback - 2); 
    lua_pushvalue(L, traceback - 1); 
    lua_pushfstring(L, "%p_%d", (void*)handle, TCP_LISTEN_CB); 
    lua_pushinteger(L, status);
    r = lua_pcall(L, 3, 0, traceback);
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

static int udp_bind(lua_State *L) { 
    uv_udp_t *w = get_udp(L, 1); 
    const char * str_add = luaL_checkstring(L, 2); 
    int port = luaL_checkinteger(L, 3); 

    struct sockaddr* realAddr = NULL;
    struct sockaddr_in addr4;
    struct sockaddr_in6 addr6;
    realAddr = get_sock_addr( str_add, port, &addr4, &addr6 );    
    int nret = uv_udp_bind(w,(const struct sockaddr*)realAddr,0); 

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

static int tcp_read_nohead(lua_State *L) {  
    uv_tcp_t * client = get_tcp(L, 1); 
    int nret = uv_read_start((uv_stream_t *)client, alloc_buffer, echo_read_nohead); 
    lua_pushinteger(L, nret);
    return 1; 
}

static int udp_read(lua_State *L) {  
    uv_udp_t * client = get_udp(L, 1); 
    int nret = uv_udp_recv_start(client, alloc_buffer, echo_read_udp); 
    lua_pushinteger(L, nret);
    return 1; 
}

static int udp_close(lua_State *L) {  
    uv_udp_t * client = get_udp(L, 1); 
    uv_close((uv_handle_t*) client, udp_close_cb);
    return 0; 
}

static int tcp_close(lua_State *L) {  
    uv_tcp_t * client = get_tcp(L, 1); 
    uv_close((uv_handle_t*) client, tcp_close_cb);
    return 0; 
}

static int tcp_write_nohead(lua_State *L) {  
    size_t len = 0;
    uv_tcp_t * client = get_tcp(L, 1); 
    const char * str = lua_tolstring(L, 2, &len); 
    assert (len <= 65535);

    write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
    ((uv_write_t*)req)->data = (void *)client;
    uint8_t * header = (uint8_t *)malloc(len);  
    memcpy(header, str, len); 

    req->buf = uv_buf_init((char *)header, len); 
    int nret = uv_write((uv_write_t*) req, (uv_stream_t *)client, &(req->buf), 1, echo_write); 

    lua_pushinteger(L, nret);
    return 1;     
}

static int udp_sendto(lua_State *L) {  
    size_t len = 0;
    uv_udp_t * client = get_udp(L, 1); 
    const char * str_add = luaL_checkstring(L, 2); 
    int port = luaL_checkinteger(L, 3);      
    const char * str = lua_tolstring(L, 4, &len); 
    assert (len <= 65535);   

    struct sockaddr* realAddr = NULL;
    struct sockaddr_in addr4;
    struct sockaddr_in6 addr6;
    realAddr = get_sock_addr( str_add, port, &addr4, &addr6 );    

    udp_send_req_t *req = (udp_send_req_t*) malloc(sizeof(udp_send_req_t));
    ((uv_udp_send_t*)req)->data = (void *)client;
    uint8_t * header = (uint8_t *)malloc(len);  
    memcpy(header, str, len); 

    req->buf = uv_buf_init((char *)header, len); 
    int nret = uv_udp_send((uv_udp_send_t*) req, client, &(req->buf), 1, realAddr, echo_write_udp); 

    lua_pushinteger(L, nret);
    return 1;     
}

static int tcp_write(lua_State *L) {  
    size_t len = 0;
    uv_tcp_t * client = get_tcp(L, 1); 
    const char * str = lua_tolstring(L, 2, &len); 
    assert (len <= 65535);

    write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
    ((uv_write_t*)req)->data = (void *)client;
    uint8_t * header = (uint8_t *)malloc(len + 2);  

    write_size(header, len); 
    memcpy(header + 2, str, len); 

    req->buf = uv_buf_init((char *)header, len + 2); 
    int nret = uv_write((uv_write_t*) req, (uv_stream_t *)client, &(req->buf), 1, echo_write); 

    lua_pushinteger(L, nret);
    return 1;     
}

static int tcp_getsockname(lua_State *L) { 
    uv_tcp_t * client = get_tcp(L, 1); 

    struct sockaddr addrlo1;
    struct sockaddr_in addrin1;  
    int addrlen1 = sizeof(addrlo1);
    char sockname1[17] = {0};
  
    int ret1 = uv_tcp_getsockname((const uv_tcp_t *)client,&addrlo1,&addrlen1);
    if(0 ==  ret1) { 
        addrin1 = *((struct sockaddr_in*)&addrlo1);
        uv_ip4_name(&addrin1,sockname1,addrlen1);
        //printf("re %s:%d From %s:%d \n",sockname1, ntohs(addrin1.sin_port),socknamepeer1, ntohs(addrinpeer1.sin_port));  

        lua_pushinteger(L, 0);
        lua_pushfstring(L, "%s", sockname1);
        lua_pushinteger(L, ntohs(addrin1.sin_port));
        return 3; 
    }
    else {
        lua_pushinteger(L, 1);
        return 1;         
    }
}

static int tcp_getpeername(lua_State *L) { 
    uv_tcp_t * client = get_tcp(L, 1); 

    struct sockaddr addrlo1;
    struct sockaddr_in addrin1;  
    int addrlen1 = sizeof(addrlo1);
    char sockname1[17] = {0};
 
    struct sockaddr addrpeer1;
    struct sockaddr_in addrinpeer1;    
    int addrlenpeer1 = sizeof(addrpeer1);
    char socknamepeer1[17] = {0};
 
    int ret1 = uv_tcp_getsockname((const uv_tcp_t *)client,&addrlo1,&addrlen1);
    int ret2 = uv_tcp_getpeername((const uv_tcp_t *)client,&addrpeer1,&addrlenpeer1);
    if(0 ==  ret1 && 0 == ret2) { 
        addrin1 = *((struct sockaddr_in*)&addrlo1);
        addrinpeer1 = *((struct sockaddr_in*)&addrpeer1);
        uv_ip4_name(&addrin1,sockname1,addrlen1);
        uv_ip4_name(&addrinpeer1,socknamepeer1,addrlenpeer1);
        //printf("re %s:%d From %s:%d \n",sockname1, ntohs(addrin1.sin_port),socknamepeer1, ntohs(addrinpeer1.sin_port));  

        lua_pushinteger(L, 0);
        lua_pushfstring(L, "%s", socknamepeer1);
        lua_pushinteger(L, ntohs(addrinpeer1.sin_port));
        return 3; 
    }
    else {
        lua_pushinteger(L, 1);
        return 1;         
    }
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
    WATCHER_METAMETHOD_TABLE_SOCKET(tcp),
    {"bind",         tcp_bind}, 
    {"listen",       tcp_listen}, 
    {"accept",       tcp_accept}, 
    {"read",         tcp_read}, 
    {"write",        tcp_write}, 
    {"read_nohead",  tcp_read_nohead}, 
    {"write_nohead", tcp_write_nohead},     
    {"close",        tcp_close}, 
    {"connect",      tcp_connect}, 
    {"getpeername",  tcp_getpeername}, 
    {"getsockname",  tcp_getsockname}, 
    {NULL, NULL}
};
METATABLE_BUILDER(tcp, WATCHER_METATABLE(tcp))
static const struct luaL_Reg uv_module_tcp[] = {
    {"new_tcp", new_tcp}, 
    {NULL, NULL}
};



static const struct luaL_Reg mt_udp[] = {
    {"__tostring", udp_tostring},
    {NULL, NULL}
};
static const struct luaL_Reg methods_udp[] = {
    WATCHER_METAMETHOD_TABLE_SOCKET(udp),
    {"bind",        udp_bind}, 
    {"sendto",      udp_sendto}, 
    {"read_nohead", udp_read}, 
    {"close",       udp_close},     
    {NULL, NULL}
};
METATABLE_BUILDER(udp, WATCHER_METATABLE(udp))
static const struct luaL_Reg uv_module_udp[] = {
    {"new_udp", new_udp}, 
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
        ADD_CONSTANT(L, TCP_WRITE_CB);

        return 1;
    }

    LUALIB_API int luaopen_uvdrv_udp(lua_State *L) {
        luaL_checkversion(L);
        CREATE_METATABLE(udp, L);
        luaL_newlib(L, uv_module_udp);

        // add constant
        ADD_CONSTANT(L, UDP_READ_CB);
        ADD_CONSTANT(L, UDP_CLOSED_CB);
        ADD_CONSTANT(L, UDP_WRITE_CB); 

        return 1;
    }    
}

