local mm = require "mm"
local binds = {}

local calls = {
  create = function(callback, self, ...)
    if type(self) ~= "table" then
      return nil, ("expected 'self' to be table, got %s)"):format(type(self))
    end
    local ref = callback(self, ...)
    --[[
    if type(ref) ~= "userdata" then
      return nil, ("expected userdata, got %s"):format(type(ref))
    end
    self.__binding = ref
    ]]
    return true
  end,
  update = function(callback, self, update_name, update_data)
    if type(self) ~= "table" then
      return nil, ("expected 'self' to be table, got %s)"):format(type(self))
    end
    if type(update_name) ~= "string" then
      return nil, ("expected 'update_name' to be string, got %s)"):format(type(self))
    end
    local ref = callback(self, update_name, update_data, self.ruleset)
    if type(ref) == "userdata" then
      self.__binding = ref
    end
    return true
  end,
  replace = function(callback, self, replacee)
    if type(self) ~= "table" then
      return nil, ("expected 'self'(replacement) to be table, got %s)"):format(type(self))
    end
    if type(self) ~= "table" then
      return nil, ("expected 'replacee' to be table, got %s)"):format(type(replacee))
    end
    if self.ruleset ~= replacee.ruleset then
      return nil, "replacement ruleset differs from replacee ruleset"
    end
    local ref = callback(self, replacee, self.ruleset)
    if type(ref) == "userdata" then
      self.__binding = ref
    end
    return true
  end,
  delete = function(callback, self)
    if type(self) ~= "table" then
      return nil, ("expected 'self' to be table, got %s)"):format(type(self))
    end
    callback(self, replacee, self.ruleset)
    return true
  end
}

return {
  bindings = binds,
  set = function(name, create, replace, update, delete)
    if type(create) == "table" and replace == nil and update == nil and delete == nil then
      local tbl = create
      create = tbl.create
      replace = tbl.replace
      update = tbl.update
      delete = tbl.delete
    end
    
    local callbacks = {
      create = create,
      replace = replace,
      update = update,
      delete = delete
    }
    
    for n,f in pairs(callbacks) do
      assert(type(f) == "function" or type(f) == nil, ("\"%s\" binding \"%s\" callback must be function or nil, was %s"):format(name, n, type(f)))
    end
    
    binds[name]=callbacks
  end,
  call = function(binding_name, call_name, ...)
    local callbacks = binds[binding_name]
    if not callbacks then return end
    local thiscall = calls[call_name]
    if not thiscall then
      error(("unknown binding call \"%s\""):format(call_name))
    end
    local ok, err = thiscall(callbacks[call_name], ...)
    if not ok then
      error(("Binding \"%s\" call \"%s\" error: %s"):format(binding_name, call_name, err))
    end
    return ok
  end
}
