local binds = {}
local Binding = {
  bindings = binds,
  require_create_userdata = false,
  require_binding = false
}
setmetatable(binds, {__index = function(t,k)
  if Binding.require_binding then
    error("missing binding for " .. tostring(k))
  end
end})
--local mm = require "mm"

local calls = {
  create = function(create_callback, self, ...)
    if type(self) ~= "table" then
      return nil, ("expected 'self' to be table, got %s)"):format(type(self))
    end
    local ref = create_callback(self, ...)
    if (Binding.require_create_userdata or ref) and type(ref) ~= "userdata" then
      return nil, ("expected userdata, got %s"):format(type(ref))
    elseif type(ref) == "userdata" then
      self.__binding = ref
    end
    return true
  end,
  update = function(update_callback, self, delta)
    if type(self) ~= "table" then
      return nil, ("expected self (current data) to be table, got %s)"):format(type(self))
    end
    if not self.name then
      return nil, "can't update nameless thing"
    end
    local ref = update_callback(self, delta)
    if type(ref) == "userdata" then
      self.__binding = ref
    end
    return true
  end,
  delete = function(delete_callback, self, ...)
    if type(self) ~= "table" then
      return nil, ("expected 'self' to be table, got %s)"):format(type(self))
    end
    assert(type(self.__binding) == "userdata", ("expected seld.__binding userdata, got %s"):format(type(self.__binding)))
    delete_callback(self.__binding, self, ...)
    return true
  end
}

function Binding.set(name, create, update, delete)
  assert(not rawget(binds, name), ("binding %s already set"):format(name))
  assert(type(name)=="string", "binding name must be a string, got " .. type(name))
  if type(create) == "table" and update == nil and delete == nil then
    local tbl = create
    create = tbl.create
    update = tbl.update
    delete = tbl.delete
  end
  
  local callbacks = {
    create = create,
    update = update,
    delete = delete
  }
  
  for n,f in pairs(callbacks) do
    assert(type(f) == "function" or type(f) == nil, ("\"%s\" binding \"%s\" callback must be function or nil, was %s"):format(name, n, type(f)))
  end
  
  binds[name]=callbacks
end
function Binding.call(binding_name, call_name, ...)
  local callbacks = binds[binding_name]
  if not callbacks then return end
  local binding_call = calls[call_name]
  if not binding_call then
    error(("unknown binding call \"%s\" for \"%s\""):format(call_name, binding_name))
  end
  if not callbacks[call_name] then
    error(("no callback for binding call \"%s\" for \"%s\""):format(call_name, binding_name))
  end
  local ok, err = binding_call(callbacks[call_name], ...)
  if not ok then
    error(("Binding \"%s\" call \"%s\" error: %s"):format(binding_name, call_name, err))
  end
  return ok
end

return Binding
