local socket    = require "uvdriver.socket"
local dns       = require "uvdriver.http.dns"

local util = {}
function util.create_connection(host, port, timeout)
    local ret, err = dns.resolve(host)
    if not ret then
        return nil, err
    end

    local ip = ret[1]
    local sock, err = socket.tcp()
    if not sock then
        return nil, err --maybe: Too many open files
    end
    if timeout then
        --sock:settimeout(timeout)
    end
    local ok, err = sock:connect(ip, port)
    if not ok then
        sock:close()
        return nil, err
    end
    return sock
end

function util.listen(ip, port, max_cnt)
    local sock, err= socket.tcp()
    if not sock then
        return nil, err
    end

    local ok, err = sock:bind(ip, port)
    if not ok then
        return nil, err
    end

    local ok, err = sock:listen(max_cnt or 100) 
    if not ok then
        return nil, err
    end
    return sock
end

return util

