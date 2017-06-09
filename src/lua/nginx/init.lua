--lua environs initializer
return function(package_loader, manager, init_bind_cfunc, ruleset_confset_cfunc)
  package.path = ""
  package.cpath= ""
  setmetatable(package.preload, {__index = function(self, name)
    local pkg = package_loader(name)
    local loader
    if pkg then
      loader = function()
        return pkg
      end
      self[name]=loader
    end
    return loader
  end})
  
  
  --GLOBALS
  Parser = require "parser"
  Ruleset = require "ruleset"
  Binding = require "binding"
  
  Binding.require_create_userdata = true
  Binding.require_binding = true
  
  mm = require "mm"
  
  if init_bind_cfunc then
    init_bind_cfunc(Binding.set)
  end
  
  local parsed_ruleset_data = {}
  local rulesets = {}
  
  function parseRulesetFile(prefix, path, ruleset_name)
    local p = Parser.new()
    
    local fullpath
    if path:match("^/") then
      fullpath = path
    elseif prefix then
      fullpath = (prefix:match("/$") and "%s%s" or "%s/%s"):format(prefix, path)
    end
    
    local ok, res = pcall(p.parseFile, p, fullpath)
    if not ok then
      return nil, (res:match("[^:]*:%d+: (.*)") or res)
    end
    
    if ruleset_name then
      res.name = ruleset_name
    end
    if not res.name then
      local filename = path:match("[^/]+$")
      local sansext = filename:match("(.*)%.[^.]+$")
      filename = sansext or filename
      res.name = filename
    end
    
    if res.name and parsed_ruleset_data[res.name] then
      return nil, ("ruleset \"%s\" already exists"):format(res.name)
    end
    parsed_ruleset_data[res.name] = res
    
    return res
  end
  
  local deferred = setmetatable({}, {__index = function(t,k)
    t[k]={}
    return t[k]
  end})
  
  function deferRulesetCreation(name, conf_ref)
    if not parsed_ruleset_data[name] then
      return nil, ("unknown ruleset \"%s\""):format(name)
    end
    table.insert(deferred[name], conf_ref)
    return true
  end
  
  function createDeferredRulesets()
    for name, def in pairs(deferred) do
      for _, ref in pairs(def) do
        local rsdata = parsed_ruleset_data[name]
        local rs = Ruleset.new(rsdata)
        ruleset_confset_cfunc(rs, ref)
        rulesets[rs.name] = rs
      end
    end
    deferred = setmetatable({}, getmetatable(deferred)) -- clear 'deferred' table
  end
  
  function shutdown(manager)
    if manager then
      for name, ruleset in pairs(rulesets) do
        print("kill a ruleset named " .. name)
        ruleset:destroy()
      end
    end
    rulesets = {}
  end
  
  function printFunctions(pattern)
    for k,v in pairs(_G) do
      if type(v) == "function" then
        if not pattern or k:match(pattern) then
          local info = debug.getinfo(v)
          print(("%s: function %s (%s %s %s:%i-%i)"):format(tostring(k), tostring(v), info.what, info.namewhat, info.short_src, info.linedefined, info.lastlinedefined))
        end
      end
    end
  end
end
