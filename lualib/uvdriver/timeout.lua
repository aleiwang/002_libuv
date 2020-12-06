local hub        = require "uvdriver.hub"
local class      = require "uvdriver.class"
local exceptions = require "uvdriver.exceptions"

local Timeout = class("TimeoutException", exceptions.BaseException)

function Timeout:_init(ms) 
    self.ms = ms
    if not self.ms or self.ms <= 0 then
        return
    end
    self.timer = hub.loop:timer()
end

function Timeout:start()
    if self.timer then
        local co = coroutine.running()
        self.timer:start(function ( inputs, outputs )
            hub:throw(co, self) 
        end, self.ms, 0)
    end
end

function Timeout:cancel()
    if self.timer then
        self.timer:stop()
    end
end

function Timeout:__tostring()
    return string.format("Timeout: %s ms", self.ms)
end

local timeout = {}

timeout.timeout = Timeout.new
function timeout.start_new(ms)
    local t = Timeout.new(ms)
    t:start()
    return t
end

function timeout.run(ms, func, ...)
    local t = Timeout.new(ms)
    t:start()
    local args = table.pack(xpcall(func, debug.traceback, ...))
    t:cancel()
    if args[1] then
        return table.unpack(args, 1, args.n)
    else
        if args[2] == t then
            return false, t
        else
            error(args[2])
        end
    end
end

function timeout.is_timeout(obj)
    return class.isinstance(obj, Timeout)
end
return timeout

