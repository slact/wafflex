local Parser = require "parser"
local Ruleset = require "ruleset"
local Binding = require "binding"
Parser.RuleComponent.generate_refs = true

local inspect = require "inspect"

--luacheck: globals redis cjson ARGV
local hmm = function(thing)
  local out = inspect(thing)
  for line in out:gmatch('[^\r\n]+') do
    redis.call("ECHO", line)
  end
end

local nextarg; do local n = 0; nextarg = function(how_many)
  local ret = {}; how_many = how_many or 1;
  for i=1,how_many do ret[i]=ARGV[n] or false end
  return table.unpack(ret)
end; end

local prefix, action, item = nextarg(3)
prefix =  prefix and prefix .. ":" or ""

Ruleset.uniqueName = function(thing, thingtbl, ruleset)
  local name, key, n
  if thing == "ruleset" then
    key = prefix .. "rulesets:n"
  else
    key = (prefix .. "ruleset:" .. ruleset.name)
  end
  n = redis.pcall("HINCRBY", key, thing .. ":n")
  name = ("%s%i"):format(thing, n)
  if redis.call("SISMEMBER", key, name) == 1 then --already exists
    return Ruleset.uniqueName(thing, thingtbl, ruleset)
  elseif thingtbl and thingtbl[name] then -- also already exists
    return Ruleset.uniqueName(thing, thingtbl, ruleset)
  else
    return name
  end
end




local actions;
actions = {
  ruleset = {
    create = function()
      local json_in, ruleset_name = nextarg(2)
      local p = Parser.new()
      local parsed, err = p:parseJSON(json_in, ruleset_name or "anonymous ruleset")
      if not parsed then
        return {0, err}
      end
      
      local rs = Ruleset.new(parsed)
      hmm(rs)
      
      return {1}
    end,
    delete = function()
      local name, ruleset_name = nextarg(2)
      error("can'd do this yet" .. name, ruleset_name)
    end
  },
  list = {
    create = function()
    
    end,
    delete = function()
    
    end
  },
  rule = {
    create = function()
    
    end,
    delete = function()
    
    end
  },
  limiter = {
    create = function()
    
    end,
    delete = function()
    
    end
  }
}

if actions[item][action] then
  return actions[item][action]()
else
  error(("unknown action %s for item %s"):format(action, item))
end
