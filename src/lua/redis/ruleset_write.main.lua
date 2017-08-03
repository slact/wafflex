local Parser = require "parser"
local Ruleset = require "ruleset"
local Binding = require "binding"
local inspect = require "inspect"
local ruleset_read = require "ruleset_read"

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

local function redis_gethash(redis_key)
  local res = redis.call("HGETALL", redis_key)
  if type(res)~="table" then return nil end
  local h, k = {}, nil
  for _, v in ipairs(res) do
    if k == nil then k=v
    else h[k]=v; k=nil end
  end
  return h
end

local function table_keys(tbl)
  local keys = {}
  for k, _ in pairs(tbl) do
    table.insert(keys, k)
  end
  return keys
end

--[[local function tcopy(tbl)
  local cpy = {}
  for k, v in pairs(tbl) do
    cpy[k]=v
  end
  return cpy
end
]]
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
    scripts =  "wafflex:scripts",
    rulesets = prefix.."rulesets",
    ruleset =  kbase,
    ruleset_pubsub = kbase..":pubsub",
    phases =   kbase..":phases",
    lists =    kbase..":lists",
    rules =    kbase..":rules",
    limiters = kbase..":limiters",
  }
  keyf = {
    list =         key.ruleset..":list:%s",
    list_rules =   key.ruleset..":list:%s:rules",
    list_refs =    key.ruleset..":list:%s:refs",
    
    rule =         key.ruleset..":rule:%s",
    rule_refs =    key.ruleset..":rule:%s:refs",
    
    limiter =      key.ruleset..":limiter:%s",
    limiter_refs = key.ruleset..":limiter:%s:refs",
    limiter_pubsub=key.ruleset..":limiter:%s:pubsub",
    
    phase =        key.ruleset..":phase:%s",
    phase_lists =  key.ruleset..":phase:%s:lists"
  }
end
genkeys(ruleset_name)

local function publish(action_name, thing_type, thing_name, thing_data)
  local msg = {
    action = action_name,
    type = thing_type,
    name = thing_name,
    data = thing_data
  }
  
  assert(action_name == "create" or action_name == "update" or action_name == "delete", "publish: invalid action " .. action_name)
  assert(thing_type == "ruleset" or thing_type == "phase" or thing_type == "limiter" or thing_type == "list" or thing_type == "rule", "publish: invalid thing type " .. thing_type)
  
  redis.call("PUBLISH", key.ruleset_pubsub, cjson.encode(msg))
end

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
      redis.call("ZINCRBY", keyf.limiter_refs:format(limiter.burst.name), 1, "limiter:"..limiter.name)
    end
    
    limiter_created[limiter.name]=true
  end,
  update = function(limiter, diff)
    local lkey = keyf.limiter:format(limiter.name)
    if redis.call("EXISTS", lkey) == 0 then error("limiter \"" .. limiter.name .. "\" does not exist") end
    
    assert(not diff.name, "don't know how to rename limiters yet")
    
    limiter.gen = tonumber(limiter.gen or 0) + 1
    diff.gen = true
    
    if diff.burst then
      redis.call("ZINCRBY", keyf.limiter_refs:format(diff.burst.old.name), -1, "limiter:"..limiter.name)
      redis.call("ZREMRANGEBYSCORE", keyf.limiter_refs:format(diff.burst.old.name), "-inf", 0)
      redis.call("ZINCRBY", keyf.limiter_refs:format(diff.burst.new.name), 1, "limiter:"..limiter.name)
    end
    
    redis_hmset(lkey, limiter, table_keys(diff))
  end,
  delete = function(limiter)
    local lkey = keyf.limiter:format(limiter.name)
    if redis.call("EXISTS", lkey) == 0 then error("limiter \"" .. limiter.name .. "\" does not exist") end
    if redis.call("ZCARD", keyf.limiter_refs:format(limiter.name)) > 0 then error("limiter \"" .. limiter.name .. "\" still in use") end
    
    if limiter.burst then
      redis.call("ZINCRBY", keyf.limiter_refs:format(limiter.burst.name), -1, "limiter:"..limiter.name)
      redis.call("ZREMRANGEBYSCORE", keyf.limiter_refs:format(limiter.burst.name), "-inf", 0)
    end
    redis.call("SREM", key.limiters, limiter.name)
    redis.call("DEL", lkey)
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
    local list_ref = "list:"..list.name
    for _, rule in ipairs(list.rules) do
      redis.call("RPUSH", list_rules_key, rule.name)
      redis.call("ZINCRBY", keyf.rule_refs:format(rule.name), 1, list_ref)
    end
    
    redis.call("SADD", key.lists, list.name)
  end,
  update = function(list, diff)
    local lkey = keyf.list:format(list.name)
    if redis.call("EXISTS", lkey) == 0 then error("list \"" .. list.name .. "\" does not exist") end
    list.gen = tonumber(list.gen or 0) + 1
    diff.gen = true
    
    assert(not diff.name, "don't know how to rename lists yet")
    
    if diff.rules then
      local list_rules_key = keyf.list_rules:format(list.name)
      local list_ref = "list:"..list.name
      redis.call("DEL", list_rules_key)
      for _, rule in ipairs(diff.rules.old) do
        local rule_refs_key = keyf.rule_refs:format(rule.name)
        redis.call("ZINCRBY", rule_refs_key, -1, list_ref)
        redis.call("ZREMRANGEBYSCORE", rule_refs_key, "-inf", 0)
      end
      
      for _, rule in ipairs(diff.rules.old) do
        redis.call("RPUSH", list_rules_key, rule.name)
        redis.call("SADD", keyf.rule_refs:format(rule.name), list_ref)
      end
      
      --redis.call("ZREMRANGEBYSCORE", list_refs_key, "-inf" 0)
      diff.rules = nil
    end
    
    redis_hmset(lkey, list, table_keys(diff))
  end,
  delete = function(list)
    local lkey = keyf.list:format(list.name)
    local list_refs_key = keyf.list_refs:format(list.name)
    if redis.call("EXISTS", lkey) == 0 then error("list \"" .. list.name .. "\" does not exist") end
    if redis.call("ZCARD", list_refs_key) ~= 0 then error("list \"" .. list.name .. "\" is still in use") end
    
    local list_rules_key = keyf.list_rules:format(list.name)
    local list_ref = "list:"..list.name
    
    for _, rule_name in ipairs(redis.call("LRANGE", list_rules_key, 0, -1)) do
      local rule_refs_key = keyf.rule_refs:format(rule_name)
      redis.call("ZINCRBY", rule_refs_key, -1, list_ref)
      redis.call("ZREMRANGEBYSCORE", rule_refs_key, "-inf", 0)
    end
    
    redis.call("SREM", key.lists, list.name)
    redis.call("DEL", lkey, list_rules_key, list_refs_key)
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
    local phase_ref = "phase:"..phase.name
    for _, list in ipairs(phase.lists) do
      redis.call("RPUSH", phase_lists_key, list.name)
      redis.call("ZINCRBY", keyf.list_refs:format(list.name), 1, phase_ref)
    end
    
    redis.call("SADD", key.phases, phase.name)
  end,
  update = function(phase, diff)
    phase.gen = tonumber(phase.gen or 0) + 1
    diff.gen = true
    assert(not diff.name, "don't know how to rename phases yet")
    
    local pkey = keyf.phase:format(phase.name)
    local phase_ref = "phase:"..phase.name
    
    if diff.lists then
      local phase_lists_key = keyf.phase_lists:format(phase.name)
      local list_refs_key
      for _, list_name in ipairs(redis.call("LRANGE", phase_lists_key, 0, -1)) do
        list_refs_key = keyf.list_refs:format(list_name)
        redis.call("ZINCRBY", list_refs_key, -1, phase_ref)
        redis.call("ZREMRANGEBYSCORE", list_refs_key, "-inf", 0)
      end
      redis.call("DEL", phase_lists_key)
      
      for _, list in ipairs(diff.lists.new) do
        list_refs_key = keyf.list_refs:format(list.name)
        redis.call("RPUSH", phase_lists_key, list.name)
        redis.call("ZINCRBY", list_refs_key, 1, phase_ref)
      end
      diff.lists = nil
    end
    
    redis_hmset(pkey, phase, table_keys(diff))
  end,
  delete = function(phase)
    local pkey = keyf.phase:format(phase.name)
    local phase_ref = "phase:"..phase.name
    
    local phase_lists_key = keyf.phase_lists:format(phase.name)
    local list_refs_keys
    for _, list_name in ipairs(redis.call("LRANGE", phase_lists_key, 0, -1)) do
      list_refs_keys = keyf.list_refs:format(list_name)
      redis.call("ZINCRBY", list_refs_keys, -1, phase_ref)
      redis.call("ZREMRANGEBYSCORE", list_refs_keys, "-inf", 0)
    end
    
    redis.call("SREM", key.phases, phase.name)
    redis.call("DEL", pkey, phase_lists_key)
  end
})

Binding.set("rule", {
  create = function(rule)
    if rule.external then return end
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
      for ref, _ in pairs(rule.refs) do
        local ref_type, ref_name = ref:match "([^:]+):(.+)"
        local refkeyf = assert(keyf[ref_type .. "_refs"], "keyf \"" .. tostring(ref_type).."_refs" .. "\" missing " .. inspect(keyf))
        redis.call("ZINCRBY", refkeyf:format(ref_name), 1, rule_ref)
      end
    end
    redis.call("SADD", key.rules, rule.name)
  end,
  update = function(rule, diff)
    local rkey = keyf.rule:format(rule.name)
    if redis.call("EXISTS", rkey) == 0 then error("rule \"" .. rule.name .. "\" does not exist") end
    assert(not diff.name, "don't know how to rename rules yet")
    rule.gen = tonumber(rule.gen or 0) + 1
    diff.gen = true
    
    if diff["if"] then
      redis.call("HSET", rkey, "if", rule["if"]:toJSON())
      diff["if"]=nil
    end
    if diff["then"] then
      redis.call("HSET", rkey, "then", rule["then"]:toJSON())
      diff["then"]=nil
    end
    if diff["else"] then
      redis.call("HSET", rkey, "else", rule["else"]:toJSON())
      diff["else"]=nil
    end
    
    if diff.refs then
      local ref_kind, ref_name, refkey
      local rule_ref = "rule:"..rule.name
      for ref, _ in pairs(diff.refs.old or {}) do
        ref_kind, ref_name = ref:match("(.+):(.+)")
        refkey = assert(keyf[ref_kind .. "_refs"]):format(ref_name)
        redis.call("ZINCRBY", refkey, -1, rule_ref)
        redis.call("ZREMRANGEBYSCORE", refkey, "-inf", 0)
      end
      
      for ref, _ in pairs(diff.refs.new or {}) do
        ref_kind, ref_name = ref:match("(.+):(.+)")
        refkey = assert(keyf[ref_kind .. "_refs"]):format(ref_name)
        redis.call("ZINCRBY", refkey, 1, rule_ref)
      end
    end
    
    redis_hmset(rkey, rule, table_keys(diff))
  end,
  delete = function(rule)
    local rkey = keyf.rule:format(rule.name)
    if redis.call("EXISTS", rkey) == 1 then error("rule \"" .. rule.name .. "\" does not exist") end
    if redis.call("ZCARD", keyf.rule_refs:format(rule.name)) > 0 then error("rule \"" .. rule.name .. "\" still in use") end
    
    local rule_ref = "rule:"..rule.name
    for ref, _ in pairs(rule.refs or {}) do
      local ref_kind, ref_name = ref:match("(.+):(.+)")
      local refkey = assert(keyf[ref_kind .. "_refs"]):format(ref_name)
      redis.call("ZINCRBY", refkey, -1, rule_ref)
      redis.call("ZREMRANGEBYSCORE", refkey, "-inf", 0)
    end
    
    redis.call("SREM", key.rules, rule.name)
    redis.call("DEL", rkey)
  end
})

Binding.set("ruleset", {
  create = function(ruleset)
    if ruleset.external then return end
    if redis.call("SISMEMBER", key.rulesets, ruleset.name) == 1 then error(("ruleset \"%s\" already exists"):format(ruleset.name)) end
    
    ruleset.gen = 0
    redis_hmset(key.ruleset, ruleset, "name", "info", "gen")
    redis.call("SADD", key.rulesets, ruleset.name)
  end,
  update = function(ruleset, diff)
    if redis.call("SISMEMBER", key.rulesets, ruleset.name) == 0 then error(("ruleset \"%s\" does not exist"):format(ruleset.name)) end
    assert(not diff.name, "don't know how to rename rulesets yet")
    assert(not diff.lists, "don't know how to update ruleset lists inline")
    assert(not diff.limiters, "don't know how to update ruleset limiters inline")
    assert(not diff.rules, "don't know how to update ruleset rules inline")
    ruleset.gen = tonumber(ruleset.gen or 0) + 1
    diff.gen = true
    
    redis_hmset(key.ruleset, ruleset, table_keys(diff))
  end,
  delete = function(ruleset)
    if redis.call("SISMEMBER", key.rulesets, ruleset.name) == 0 then error(("ruleset \"%s\" does not exist"):format(ruleset.name)) end
    --TODO: is this ruleset in use?
    redis.call("SREM", key.rulesets, ruleset.name)
    assert(redis.call("SCARD", key.phases) == 0, "some phases still present in ruleset")
    assert(redis.call("SCARD", key.list) == 0, "some lists still present in ruleset")
    assert(redis.call("SCARD", key.rule) == 0, "some rules still present in ruleset")
    assert(redis.call("SCARD", key.limiter) == 0, "some limiters still present in ruleset")
    
    redis.call("DEL", key.ruleset)
    
  end
})

local function get_external_ruleset(parsed_ruleset)
  if not ruleset_name or #ruleset_name == 0 then return nil, "no ruleset name given" end
  if redis.call("EXISTS", key.ruleset) == 0 then
    return nil, ("ruleset \"%s\" does not exist"):format(ruleset_name)
  end
  local rs = redis_gethash(key.ruleset)
  if parsed_ruleset then
    for k,v in pairs(parsed_ruleset) do
      rs[k]=v
    end
  end
  rs.external = true
  rs = Ruleset.new(rs)
  return rs
end

local function check_existence_for_update(name, keyfmt, description)
  if not name or #name == 0 then
    return nil, description.." name missing"
  end
  if redis.call("EXISTS", keyfmt:format(name)) == 0 then
    return nil, ("%s \"%s\" does not exist"):format(description, name)
  end
  return true
end

local parser_update_opts = { external = {
  list = function(parser, name)
    return redis.call("EXISTS", keyf.list:format(name)) == 1
  end,
  rule = function(parser, name)
    local json = ruleset_read(prefix, ruleset_name, "rule", name, true)
    local rule = parser:parseJSON("rule", json, "loaded rule", 1)
    parser.ruleset.rules[rule.name]=rule
    return rule
  end,
  limiter = function(parser, name)
    local json = ruleset_read(prefix, ruleset_name, "limiter", name, true)
    local lim = parser:parseJSON("limiter", json, "loaded limiter", 1)
    parser.ruleset.limiters[lim.name]=lim
    if lim.burst then
      parser:getLimiter(lim.burst)
    end
    return lim
  end
}}

local function run_update_command(what, update_method_name, thing_name, keyfmt, extra_fn)
  local ret, parsed, rs, err
  if what ~= "ruleset" then
    ret, err = check_existence_for_update(thing_name, keyfmt, what)
    if not ret then return {0, err} end
  else
    thing_name = ruleset_name
  end
  
  local parser = Parser.new(parser_update_opts)
  local old = parser:get(what, thing_name)
  
  local json_in = nextarg()
  parsed, err = parser:parseJSON(what, json_in, "rule update")
  if not parsed then return {0, err} end
  hmm(old)
  hmm("AND NOW...")
  hmm(parser.ruleset)
  hmm("also")
  hmm(parsed)
  
  rs, err = get_external_ruleset(parser.ruleset)
  if not rs then return {0, err} end
  hmm("...kay then")
  
  
  if extra_fn then
    ret, err = extra_fn(rs, parsed)
    if not ret then return {0, err or "extra_fn failed"} end
  end
  
  if #thing_name == 0 and parsed then thing_name = parsed.name end
  if not thing_name or #thing_name == 0 then
    return {0, ("missing %s name"):format(what)}
  end
    
  if next(parsed) then
    ret, err = rs[update_method_name](rs, thing_name, parsed)
    if not ret then
      return {0, err}
    end
  else
    return {0, "nothing to update"}
  end
  
  publish("update", what, thing_name, json_in)
  
  return {1}
end


local actions
actions = {
  ruleset = {
    create = function()
      if #ruleset_name > 0 and redis.call("EXISTS", key.ruleset) == 1 then
        return {0, ("ruleset \"%s\" already exists"):format(ruleset_name)}
      end
      
      local json_in = nextarg()
      
      local p = Parser.new()
      local parsed, err = p:parseJSON("ruleset", json_in, ruleset_name or "anonymous ruleset", true)
      if not parsed then
        return {0, err}
      end
      if #ruleset_name > 0 then
        parsed.name = ruleset_name
      end
      local rs = Ruleset.new(parsed)
      return rs and {1} or {0, "failed to create ruleset"}
    end,
    update = function()
      return run_update_command("ruleset", "updateRuleset", nextarg())
    end,
    delete = function()
      return {0, "can't do this yet"}
    end
  },
  list = {
    create = function()
      local list_name, json_in = nextarg(2)
      if not list_name then list_name = "" end
      
      if #list_name > 0 and redis.call("EXISTS", keyf.list:format(list_name)) == 1 then
        return {0, ("list \"%s\" already exists"):format(list_name)}
      end
      
      local p = Parser.new()
      local parsed, err = p:parseJSON("list", json_in, list_name)
      if not parsed then
        return {0, err}
      end
      
      local list = Ruleset.newList(parsed)
      publish("create", "list", list.name, list:toJSON())
      return list and {1} or {0, "failed to create list"}
    end,
    update = function()
      return run_update_command("list", "updateList", nextarg(), keyf.list)
    end,
    delete = function()
      return {0, "can't do this yet"}
    end
  },
  rule = {
    create = function()
      local rule_name, json_in = nextarg(2)
      if not rule_name then rule_name = "" end
      
      if #rule_name > 0 and redis.call("EXISTS", keyf.rule:format(rule_name)) == 1 then
        return {0, ("rule \"%s\" already exists"):format(rule_name)}
      end
      
      local ruleset
      local parsed, err = Parser.new():parseJSON("rule", json_in, rule_name, true)
      if not parsed then return {0, err} end
      
      ruleset, err = get_external_ruleset()
      if not ruleset then return {0, err} end
      
      if not parsed.name and #rule_name > 0 then parsed.name = rule_name end
      
      local new_rule = ruleset:addRule(parsed)
      if new_rule then
        publish("create", "rule", new_rule.name, new_rule:toJSON())
        return {1}
      else
        return {0, "failed to create rule"}
      end
    end,
    update = function()
      return run_update_command("rule", "updateRule", nextarg(), keyf.rule)
    end,
    delete = function()
      --local name = nextarg()
      return {0, "can't do this yet"}
    end
  },
  limiter = {
    create = function()
      local json_in, limiter_name = nextarg(2)
      if not limiter_name then limiter_name = "" end
      
      if #limiter_name > 0 and redis.call("EXISTS", keyf.rule:format(#limiter_name)) == 1 then
        return {0, ("limiter \"%s\" already exists"):format(#limiter_name)}
      end
      
      local p = Parser.new()
      local parsed, err = p:parseJSON("limiter", json_in, #limiter_name)
      if not parsed then
        return {0, err}
      end
      
      local new_limiter = Ruleset.newLimiter(parsed)
      if new_limiter then
        publish("create", "limiter", new_limiter.name, new_limiter:toJSON())
        return {1}
      else
        return {0, "failed to create rule"}
      end
    end,
    update = function()
      return run_update_command("limiter", "updateLimiter", nextarg(), keyf.limiter)
    end,
    delete = function()
      --local name = nextarg()
      return {0, "can't do this yet"}
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
