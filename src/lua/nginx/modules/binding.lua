local mm = require "mm"
local binds = {}
return {
  bindings = binds,
  set = function(name, create, replace, update, delete)
    binds[name]={
      create = create,
      replace = replace,
      update = update,
      delete = delete
    }
  end,
  
  call = {
    create = function(name, self)
      if not binds[name] then return end
      local ref = binds[name].create(self, self.ruleset)
      assert(type(ref) == "userdata", ("%s 'create' binding expected userdata, got %s"):format(name, type(ref)))
      self.__binding = ref
    end,
    update = function(name, self, update_name, update_data)
      if not binds[name] then return end
      local ref = binds[name].update(self, update_name, update_data, self.ruleset)
      if type(ref) == "userdata" then
        self.__binding = ref
      end
    end,
    replace = function(name, self, replacee)
      if not binds[name] then return end
      assert(self.ruleset == replacee.ruleset, "replacement ruleset differs from replacee ruleset")
      local ref = binds[name].replace(self, replacee, self.ruleset)
      if type(ref) == "userdata" then
        self.__binding = ref
      end
    end,
    delete = function(name, self)
      if not binds[name] then return end
      local ref = binds[name].delete(self, replacee, self.ruleset)
    end
  }
}
