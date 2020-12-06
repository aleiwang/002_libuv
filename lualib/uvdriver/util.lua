local hub        = require "uvdriver.hub"
local coroutines = require "uvdriver.coroutines"
local exceptions = require "uvdriver.exceptions"
local uv         = require "uvdrv.c"   

local kill_error = exceptions.KillError.new()

local util = {}

util.kill_error = kill_error

function util.now()
    return hub.loop:now()
end

function util.sleep(ms)
    if ms < 0 then
        ms = 0
    end

    hub:wait(hub.loop:timer(), ms, 0) 
end

local stats = {
    running = coroutines.running,
    cached = coroutines.cached,
    total = coroutines.total
}

function util.stats(item)
    if item == "all" then
        local t = {}
        for k,f in pairs(stats) do
            t[k] = f()
        end
        return t
    end

    local f = stats[item]
    if f then
        return f()
    end
    return nil
end

util.check_coroutine = coroutines.check

function util.spawn(f, ...)
    local co = coroutines.create(f)
    hub.loop:run_callback_prepare(coroutines.resume, co, ...)
    return co
end

function util.kill(co)
    hub.loop:run_callback_prepare(hub.throw, hub, co, kill_error)
end

function util.start(f, ...)
    util.spawn(f, ...)
    util.wait_infinite()
end

function util.waiter()
    return hub:waiter()
end

function util.get_hub()
    return hub
end

function util.run()
    hub:run(uv.UV_RUN_NOWAIT)
end

function util.wait()
    hub:run(uv.UV_RUN_ONCE)
end

function util.wait_infinite()
    hub:run(uv.UV_RUN_DEFAULT)
end

return util

