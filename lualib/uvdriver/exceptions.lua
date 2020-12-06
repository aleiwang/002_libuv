local class = require "uvdriver.class"

local BaseException = class("BaseException")
function BaseException:__tostring()
    return "BaseException"
end

local CancelWaitError = class("CancelWaitError", BaseException)
function CancelWaitError:_init(msg)
    self.msg = msg
end

function CancelWaitError:__tostring()
    if self.msg then
        return "CancelWaitError:" .. self.msg
    end
    return "CancelWaitError"
end

local KillError = class("KillError", BaseException)
function KillError:__tostring()
    return "KillError"
end

local all = {}
all.BaseException = BaseException
all.CancelWaitError = CancelWaitError 
all.KillError = KillError

function all.is_exception(exception)
    return class.isinstance(exception, BaseException)
end

return all

