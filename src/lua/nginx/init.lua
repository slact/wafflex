--lua environs initializer

--luacheck: globals Parser Ruleset Binding
--luacheck: globals parseRulesetFile deferRulesetCreation createDeferredRulesets
--luacheck: globals deferRulesetRedisLoad loadDeferredRedisRulesets
--luacheck: globals shutdown printFunctions

--luacheck: globals getRedisRulesetJSON redisRulesetSubscribe redisRulesetUnsubscribe --external crud

--setup package loading
local package_loader
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


return function(package_loader_cfunc, manager, init_bind_cfunc, ruleset_confset_cfunc)
  package_loader = package_loader_cfunc
  
  local rulesets = {}
  
  Parser = require "parser"
  Ruleset = require "ruleset"
  Binding = require "binding"
  local mm = require "mm"
  
  
  do
    local ruleset_n = 0
    local function uniqueName(thing, thingtbl, ruleset)
      local name
      if ruleset then
        if not ruleset.__n then
          ruleset.__n = 0
        else
          ruleset.__n = ruleset.__n + 1
        end
        name = ("%s%i"):format(thing, ruleset.__n)
        if thingtbl[name] then --oh no it's not unique. try again
          return uniqueName(thing, thingtbl, ruleset)
        else
          return name
        end
      else
        assert(thing == "ruleset")
        ruleset_n = ruleset_n + 1
        name = "ruleset"..ruleset_n
        if rulesets[name] then  --oh no it's not unique. try again
          return uniqueName(thing, thingtbl, ruleset)
        else
          return name
        end
      end
    end
    Ruleset.uniqueName = uniqueName
  end
  
  
  Binding.require_create_userdata = true
  Binding.require_binding = true
  
  --local mm = require "mm"
  
  if init_bind_cfunc then
    init_bind_cfunc(Binding.set)
  end
  
  if manager then
    local parsed_ruleset_data = {}
    
    function parseRulesetFile(prefix, path, ruleset_name)
      local p = Parser.new()
      
      local fullpath
      if path:match("^/") then
        fullpath = path
      elseif prefix then
        fullpath = (prefix:match("/$") and "%s%s" or "%s/%s"):format(prefix, path)
      end
      
      local res, err = p:parseFile(fullpath)
      if not res then
        return nil, err
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
  
    function deferRulesetCreation(name, conf_ptr)
      if not parsed_ruleset_data[name] then
        return nil, ("unknown ruleset \"%s\""):format(name)
      end
      table.insert(deferred[name], conf_ptr)
      return true
    end
    
    local function createRuleset(parsed_data)
      local rs = Ruleset.new(parsed_data)
      assert(rulesets[rs.name] == nil)
      rulesets[rs.name] = rs
      return rs
    end
    
    function createDeferredRulesets()
      for name, def in pairs(deferred) do
        local ruleset = createRuleset(parsed_ruleset_data[name])
        for _, conf_ptr in pairs(def) do
          ruleset_confset_cfunc(ruleset, conf_ptr)
        end
      end
      deferred = setmetatable({}, getmetatable(deferred)) -- clear 'deferred' table
    end
    
    local deferred_redis_rulesets = setmetatable({}, {__index = function(t,k)
      t[k]={}
      return t[k]
    end})
    
    function deferRulesetRedisLoad(name, loc_conf_ptr, conf_ptr)
      table.insert(deferred_redis_rulesets[name], {lcf_ptr=loc_conf_ptr, rcf_ptr = conf_ptr})
      return true
    end
    
    function loadDeferredRedisRulesets()
      local co = coroutine.wrap(function()
        local ruleset_json, parser, parsed, ruleset, err
        for name, rs in pairs(deferred_redis_rulesets) do
          for _, config in ipairs(rs) do
            if rulesets[name] then
              ruleset = rulesets[name]
            else
              redisRulesetSubscribe(config.lcf_ptr, name)
              ruleset_json, err = getRedisRulesetJSON(config.lcf_ptr, name)
              if not ruleset_json then
                redisRulesetUnsubscribe(config.lcf_ptr, name)
                error(err)
              end
              print(ruleset_json)
              parser = Parser.new()
              parsed, err = parser:parseJSON("ruleset", ruleset_json, name, true)
              if not parsed then error(err) end
              ruleset = createRuleset(parsed)
              mm(ruleset)
            end
            ruleset_confset_cfunc(ruleset, config.rcf_ptr)
          end
        end
      end)
      co()
    end
  end
  
  function shutdown(is_manager)
    if is_manager then
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
