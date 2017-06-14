local Parser = require "parser"
local Ruleset = require "ruleset"
local Binding = require "binding"
local inspect = require "inspect"

--luacheck: globals redis cjson ARGV unpack
local hmm = function(thing)
  local out = inspect(thing)
  for line in out:gmatch('[^\r\n]+') do
    redis.call("ECHO", line)
  end
end

Ruleset.RuleComponent.generate_refs = true

local tunpack = table.unpack or unpack
local function redis_hmset(key, tbl, ...)
  local flat = {}
  for _, k in ipairs{...} do
    if tbl[k] then
      table.insert(flat, k)
      table.insert(flat, tbl[k])
    end
  end
  if #flat > 0 then
    redis.call("HMSET", key, tunpack(flat))
  end
end

local nextarg; do local n = 0; nextarg = function(how_many)
  local ret = {}; how_many = how_many or 1;
  for i=1,how_many do ret[i]=ARGV[n+i] or false end
  n = n + how_many
  return tunpack(ret)
end; end

local prefix, action, item, ruleset_name = nextarg(4)
prefix = #prefix>0 and prefix .. ":" or ""

local kbase, key, keyf;
local function genkeys(new_ruleset_name)
  kbase = ("%sruleset:%s"):format(prefix, new_ruleset_name)
  key = {
    rulesets = prefix.."rulesets",
    ruleset =  kbase,
    phases =   kbase..":phases",
    lists =    kbase..":lists",
    rules =    kbase..":rules",
    limiters = kbase..":limiters"
  }
  keyf = {
    list =         key.ruleset..":list:%s",
    list_rules =   key.ruleset..":list:%s:rules",
    list_refs =    key.ruleset..":list:%s:refs",
    
    rule =         key.ruleset..":rule:%s",
    rule_refs =    key.ruleset..":rule:%s:refs",
    
    limiter =      key.ruleset..":limiter:%s",
    limiter_refs = key.ruleset..":limiter:%s:refs",
    
    phase =        key.ruleset..":phase:%s",
    phase_lists =  key.ruleset..":phase:%s:lists"
  }
end
genkeys(ruleset_name)

Ruleset.uniqueName = function(thing, thingtbl, ruleset)
  local name, thing_key, n, set_key
  if thing == "ruleset" then
    thing_key = prefix .. "rulesets:n"
    set_key = key.rulesets
  else
    thing_key = (prefix .. "ruleset:" .. ruleset.name)
    if     thing == "phase" then
      set_key = key.phases
    elseif thing == "list" then
      set_key = key.lists
    elseif thing == "rule" then
      set_key = key.rules
    elseif thing == "limiter" then
      set_key = key.limiters
    end
  end
  n = redis.call("HINCRBY", thing_key, thing .. ":n", 1)
  name = ("%s%i"):format(thing, n)
  
  if redis.call("SISMEMBER", set_key, name) == 1 then --already exists
    return Ruleset.uniqueName(thing, thingtbl, ruleset)
  elseif thingtbl and thingtbl[name] then -- also already exists
    return Ruleset.uniqueName(thing, thingtbl, ruleset)
  else
    if thing == "ruleset" then genkeys(name) end
    return name
  end
end

local limiter_created = {} --needed because limiters can reference other limiters
Binding.set("limiter", {
  create = function(limiter)
    if limiter.external then return end
    if limiter_created[limiter.name] then return end
    local lkey = keyf.limiter:format(limiter.name)
    if redis.call("EXISTS", lkey) == 1 then error("limiter \"" .. limiter.name .. "\" already exists") end
    limiter.gen = 0
    redis_hmset(lkey, limiter, "name", "info", "gen", "interval", "limit", "sync-steps", "burst-expire")
    if limiter.burst then
      redis.call("HSET", lkey, "burst", limiter.burst.name)
    end
    redis.call("SADD", key.limiters, limiter.name)
    
    if limiter.burst then
      redis.call("SADD", keyf.limiter_refs:format(limiter.burst.name), "limiter:"..limiter.name)
    end
    
    limiter_created[limiter.name]=true
  end
})

Binding.set("list", {
  create = function(list)
    if list.external then return end
    local lkey = keyf.list:format(list.name)
    if redis.call("EXISTS", lkey) == 1 then error("list \"" .. list.name .. "\" already exists") end
    list.gen = 0
    redis_hmset(lkey, list, "name", "info", "gen")
    
    local list_rules_key = keyf.list_rules:format(list.name)
    local list_refs_key = keyf.list_refs:format(list.name)
    local list_ref = "list:"..list.name
    for _, rule in ipairs(list.rules) do
      redis.call("RPUSH", list_rules_key, rule.name)
      redis.call("ZINCRBY", list_refs_key, 1, rule.name)
      redis.call("SADD", keyf.rule_refs:format(rule.name), list_ref)
    end
    
    redis.call("SADD", key.lists, list.name)
  end
})

Binding.set("phase", {
  create = function(phase)
    if phase.external then return end
    local pkey = keyf.phase:format(phase.name)
    if redis.call("EXISTS", pkey) == 1 then error("phase \"" .. phase.name .. "\" already exists") end
    phase.gen = 0
    redis_hmset(pkey, phase, "name", "info", "gen")
    
    local phase_lists_key = keyf.phase_lists:format(phase.name)
    for _, list in ipairs(phase.lists) do
      redis.call("RPUSH", phase_lists_key, list.name)
    end
    
    redis.call("SADD", key.phases, phase.name)
  end
})

Binding.set("rule", {
  create = function(rule)
    local rkey = keyf.rule:format(rule.name)
    if redis.call("EXISTS", rkey) == 1 then error("rule \"" .. rule.name .. "\" already exists") end
    rule.gen = 0
    redis_hmset(rkey, rule, "name", "info", "gen", "key")
    redis.call("HSET", rkey, "if", rule["if"]:toJSON())
    if rule["then"] then
      redis.call("HSET", rkey, "then", rule["then"]:toJSON())
    end
    if rule["else"] then
      redis.call("HSET", rkey, "else", rule["else"]:toJSON())
    end
    
    if rule.refs then
      local rule_ref = "rule:"..rule.name
      for ref_type, ref in pairs(rule.refs) do
        local refkeyf
        if ref_type == "limiter" then
          refkeyf = keyf.limiter_refs
        elseif ref_type == "rule" then
          refkeyf = keyf.rule_refs
        else
          assert("can't handle ref tyoe " .. ref_type)
        end
        for _, ref_name in ipairs(ref) do
          redis.call("SADD", refkeyf:format(ref_name), rule_ref)
        end
      end
    end
    
    redis.call("SADD", key.rules, rule.name)
  end
})

Binding.set("ruleset", {
  create = function(ruleset)
    if redis.call("SISMEMBER", key.rulesets, ruleset.name) == 1 then error(("ruleset \"%s\" already exists"):format(ruleset.name)) end
    
    ruleset.gen = 0
    redis_hmset(key.ruleset, ruleset, "name", "info", "gen")
    redis.call("SADD", key.rulesets, ruleset.name)
  end
})

local actions
actions = {
  ruleset = {
    create = function()
      if #ruleset_name > 0 and redis.call("EXISTS", key.ruleset) == 1 then
        return {0, ("ruleset \"%s\" already exists"):format(ruleset_name)}
      end
      
      local json_in = nextarg(1)
      
      local p = Parser.new()
      local parsed, err = p:parseJSON("ruleset", json_in, ruleset_name or "anonymous ruleset", true)
      if not parsed then
        return {0, err}
      end
      if #ruleset_name > 0 then
        parsed.name = ruleset_name
      end
      
      local rs = Ruleset.new(parsed)
      hmm(rs)
      
      return {1}
    end,
    delete = function()
      local name = nextarg(1)
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

if type(actions[item]) == "function" then
  actions[item](action)
elseif actions[item][action] then
  return actions[item][action]()
else
  error(("unknown action %s for item %s"):format(action, item))
end
