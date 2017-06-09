local handlers = {}
local rawget = rawget
--luacheck: globals setAlertHandler getAlertHandler
function setAlertHandler(name, callback)
  rawset(handlers, name, callback)
end
function getAlertHandler(name, data_ptr)
  return rawget(handlers, name)
end
