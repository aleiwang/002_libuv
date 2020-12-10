local class 	= require "uvdriver.class"
local c    	    = require "uvdrv.c"   
local tcp       = require "uvdrv.tcp" 
local udp       = require "uvdrv.udp" 
local hub       = require "uvdriver.hub"
local timeout   = require "uvdriver.timeout"
local SocketUdp = class("SocketUdp")
local SocketTcp = class("SocketTcp", SocketUdp)
local tpack     = table.pack
local tunpack   = table.unpack


local function _wait(watcher, ms)
    local t
    if ms and ms > 0 then
        t = timeout.start_new(ms)
    end

    local ok, excepiton = xpcall(hub.wait, debug.traceback, hub, watcher)
    if not ok then
        print(ok, excepiton)
    end
    if t then
        t:cancel()
    end
    return ok, excepiton
end

function SocketUdp:_init(loop)
    loop = loop or hub.loop 
    self.loop = loop
    local w = udp["new_udp"](self.loop.cobj)  
    self.cobj = w
    self:init_watchers() 
    print("create ", tostring(self))
end


function SocketUdp:init_watchers()
    self.watcher_read    = hub.loop:socket_watcher(self.cobj, udp.UDP_READ_CB) 
    self.watcher_close   = hub.loop:socket_watcher(self.cobj, udp.UDP_CLOSED_CB) 
    self.watcher_write   = hub.loop:socket_watcher(self.cobj, udp.UDP_WRITE_CB) 
end

function SocketUdp:id()
    return self.cobj:id()
end

function SocketUdp:is_active()
    return self.cobj:is_active()
end

function SocketUdp:__tostring()
    return tostring("udp:" .. c.topointer(self.cobj))
end

function SocketUdp:__gc()
    self:stop()
end

function SocketUdp:stop()
    self:close()
end

function SocketUdp:close()  
    self.cobj:close() 
end

function SocketUdp:bind(strIp, nPort) 
    local err = self.cobj:bind(strIp, nPort) 
    return err == 0, err 
end

function SocketUdp:sendto( ip, port, lstring ) 
    local cobj = self.cobj
    while true do
        local err = self.cobj:sendto( ip, port, lstring  )  
        if err ~= 0 then 
            return nil, err
        end

        local ok, exception = _wait(self.watcher_write, self.timeout) 
        if not ok then
            return nil, exception
        else 
            return true, exception[1], exception[2] -- statu , len
        end
    end     
end

function SocketUdp:read()  
    local cobj = self.cobj
    while true do
        local err = self.cobj:read_nohead() 
        if err ~= 0 then 
            return nil, err
        end

        local ok, exception = _wait(self.watcher_read, self.timeout)
        if not ok then
            return nil, exception
        else 
            return true,exception[1] -- read data 
        end
    end    
end

function SocketTcp:_init(loop)
    loop = loop or hub.loop 
    self.loop = loop
    local w = tcp["new_tcp"](self.loop.cobj)  
    self.cobj = w
    self:init_watchers() 
    print("create ", tostring(self))
end

function SocketTcp:__tostring()
    return tostring("tcp:" .. c.topointer(self.cobj))
end

function SocketTcp:init_watchers()
    self.watcher_listen  = hub.loop:socket_watcher(self.cobj, tcp.TCP_LISTEN_CB) 
    self.watcher_read    = hub.loop:socket_watcher(self.cobj, tcp.TCP_READ_CB) 
    self.watcher_close   = hub.loop:socket_watcher(self.cobj, tcp.TCP_CLOSED_CB) 
    self.watcher_connect = hub.loop:socket_watcher(self.cobj, tcp.TCP_CONNECT_RET_CB) 
    self.watcher_write   = hub.loop:socket_watcher(self.cobj, tcp.TCP_WRITE_CB) 
end

function SocketTcp:listen( nMaxLen ) 
    local err = self.cobj:listen(nMaxLen) 
    return err == 0, err 
end

function SocketTcp:connect(ip, port) 
    while true do
        local err = self.cobj:connect(ip, port)
        if err ~= 0 then
            return nil, err
        end

        local ok, exception = _wait(self.watcher_connect, self.timeout)
        if not ok then
            return nil, exception
        else 
            return true,exception[1] -- error no 
        end
    end    
end

function SocketTcp:getpeername()  
    return self.cobj:getpeername()
end 

function SocketTcp:accept()  
    local newTcp = SocketTcp.new( self.loop ) 
    local err
    while true do
        err = self.cobj:accept(newTcp.cobj)
        if err == 0 then
            print("accept success") 
            break
        end

        local ok, exception = _wait(self.watcher_listen, self.timeout)
        if not ok then
            return nil, exception
        end
    end

    return true, newTcp
end

function SocketTcp:write( lstring ) 
    local cobj = self.cobj
    while true do
        local err = self.cobj:write_nohead( lstring )  
        if err ~= 0 then 
            return nil, err
        end

        local ok, exception = _wait(self.watcher_write, self.timeout) 
        if not ok then
            return nil, exception
        else 
            return true,exception[1], exception[2] --  statu , len
        end
    end     
end

function SocketTcp:read_withhead()  
    local cobj = self.cobj
    while true do
        local err = self.cobj:read() 
        if err ~= 0 then 
            return nil, err
        end

        local ok, exception = _wait(self.watcher_read, self.timeout)
        if not ok then
            return nil, exception
        else 
            return true,exception[1] -- read data 
        end
    end    
end

function SocketTcp:write_withhead( lstring ) 
    local cobj = self.cobj
    while true do
        local err = self.cobj:write( lstring )  
        if err ~= 0 then 
            return nil, err
        end

        local ok, exception = _wait(self.watcher_write, self.timeout) 
        if not ok then
            return nil, exception
        else 
            return true,exception[1], exception[2] -- statu , len
        end
    end        
end

local M = {
    tcp = SocketTcp.new,
    udp = SocketUdp.new, 
}

return M 
