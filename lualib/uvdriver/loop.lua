local class 	= require "uvdriver.class"
local c    	    = require "uvdrv.c"   
local tcp       = require "uvdrv.tcp" 
local Watcher 	= class("Watcher")
local TcpWatcher= class("TcpWatcher", Watcher)
local Loop 		= class("Loop") 

local tpack = table.pack
local tunpack = table.unpack
local function wrap_callback(f, ...)
    if select("#", ...) > 0 then
        local args = tpack(...)
        return function()
            return xpcall(f, debug.traceback, tunpack(args, 1, args.n))
        end
    else
        return function()
            return xpcall(f, debug.traceback)
        end
    end
end

function Watcher:_init(name, loop)
    self.loop = loop
    local w = c["new_" .. name](self.loop.cobj)  
    self.cobj = w
    self._cb = nil
    self._args_input = nil
    self._args_output = nil
end

function Watcher:id()
    return self.cobj:id()
end

function Watcher:start(func, ...)
    assert(self._cb == nil, self.cobj)
    self._cb = func
    if select("#", ...) > 0 then
        self._args_input = tpack(...)
    end    

    self.cobj:start(...)  
end

function Watcher:stop()
    self._cb = nil
    self._args_input = nil
    self._args_output = nil
    self.cobj:stop()
end

function Watcher:run_callback( ... ) 
    if select("#", ...) > 0 then
        self._args_output = tpack(...)
    end   	
    local ok, msg = xpcall(self._cb, debug.traceback, self._args_input, self._args_output)
    if not ok then
        self.loop:handle_error(self, msg)
    end
end

function Watcher:is_active()
    return self.cobj:is_active()
end

function Watcher:__tostring()
    return tostring(self.cobj)
end

function Watcher:__gc()
    self:stop()
end


function TcpWatcher:_init(name, loop)
    self.loop = loop
    local w = tcp["new_" .. name](self.loop.cobj)  
    self.cobj = w
    self._cb = nil
    self._args_input = nil
    self._args_output = nil
end

function TcpWatcher:start(func, ...)
    assert(self._cb == nil, self.cobj)
    self._cb = func
    if select("#", ...) > 0 then
        self._args_input = tpack(...)
    end 
end

function TcpWatcher:onclosed()
    self._cb = nil
    self._args_input = nil
    self._args_output = nil
end

function TcpWatcher:stop()
    self:close()
end

function TcpWatcher:bind(strIp, nPort) 
    return self.cobj:bind(strIp, nPort) 
end

function TcpWatcher:connect(strIp, nPort) 
    return self.cobj:connect(strIp, nPort) 
end

function TcpWatcher:listen( nMaxLen ) 
    return self.cobj:listen(nMaxLen) 
end

function TcpWatcher:accept()  
    local newTcp = self.loop:tcp() 
    return newTcp, self.cobj:accept(newTcp.cobj) 
end

function TcpWatcher:read()  
    return self.cobj:read() 
end

function TcpWatcher:close()  
    self.cobj:close() 
end

function TcpWatcher:write( lstring ) 
    return self.cobj:write( lstring )  
end

function Loop:_init()
    self.cobj = c.default_loop() 
    self.watchers = setmetatable({}, {__mode="v"})

    -- register prepare
    -- idle -> prepare -> check 
    self._callbacks_idle = {}
    self._callbacks_prepare = {}
    self._callbacks_check = {}

    self._idle = self:_create_watcher("idle")
    self._idle:start(function(tbInputArgs, tbOutPutArgs) self:_run_callback(self._callbacks_idle, tbInputArgs, tbOutPutArgs) end)

    self._prepare = self:_create_watcher("prepare")
    self._prepare:start(function(tbInputArgs, tbOutPutArgs) self:_run_callback(self._callbacks_prepare, tbInputArgs, tbOutPutArgs) end) 

    self._check = self:_create_watcher("check")
    self._check:start(function(tbInputArgs, tbOutPutArgs) self:_run_callback(self._callbacks_check, tbInputArgs, tbOutPutArgs) end)  

    self._signal = self:_create_watcher("signal")
    self._signal:start(function(tbInputArgs, tbOutPutArgs) if tbOutPutArgs[1] == c.SIGINT then self:stop() end  end, c.SIGINT)  
end

function Loop:_run_callback(ref_tb_cb, tbInputArgs, tbOutPutArgs) 
    while #ref_tb_cb > 0 do
        local callbacks = ref_tb_cb

        self._callbacks_idle = (self._callbacks_idle == ref_tb_cb) and {} or self._callbacks_idle 
        self._callbacks_prepare = (self._callbacks_prepare == ref_tb_cb) and {} or self._callbacks_prepare 
        self._callbacks_check = (self._callbacks_check == ref_tb_cb) and {} or self._callbacks_check 
        ref_tb_cb = {}

        for _, cb in ipairs(callbacks) do
            local ok, msg = cb()
            if not ok then
                self:handle_error(self._prepare, msg)
            end
        end
    end
end

function Loop:run(flags)
    self.cobj:run(flags, self.callback, self)
end

function Loop:now()
    return self.cobj:now()
end

function Loop:stop() 
    self.cobj:stop()
end 

function Loop:_add_watchers(w)
    self.watchers[w:id()] = w
end

function Loop:_create_watcher(name, ...)
    local o 
    if name == "tcp" then 
        o = TcpWatcher.new(name, self, ...)
    else 
        o = Watcher.new(name, self, ...)
    end 
    self:_add_watchers(o)
    return o
end

function Loop:timer()
    return self:_create_watcher("timer")
end

function Loop:signal()
    return self:_create_watcher("signal")
end

function Loop:tcp()
    return self:_create_watcher("tcp")
end

function Loop:callback(id, ...)
    local w = assert(self.watchers[id], id)
    w:run_callback(...)
end

function Loop:run_callback_prepare(func, ...)
    self._callbacks_prepare[#self._callbacks_prepare + 1] = wrap_callback(func, ...)
end

function Loop:run_callback_idle(func, ...)
    self._callbacks_idle[#self._callbacks_idle + 1] = wrap_callback(func, ...)
end

function Loop:run_callback_check(func, ...) 
    self._callbacks_check[#self._callbacks_check + 1] = wrap_callback(func, ...)
end

function Loop:handle_error(watcher, msg)
    print("error:", watcher, msg)
end


local loop = {}
function loop.new()
    local obj = Loop.new()

    for k,v in pairs(c) do
        if type(v) ~= "function" then
            obj[k] = v
        end
    end

    for k,v in pairs(tcp) do 
        if type(v) ~= "function" then
            obj[k] = v
        end
    end

    return obj
end

return loop

