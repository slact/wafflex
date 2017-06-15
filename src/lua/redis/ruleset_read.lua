
--luacheck: globals redis cjson ARGV
local prefix =        ARGV[1]
local item =          ARGV[2]
local ruleset_name =  ARGV[3]
local item_name =     ARGV[4]

local encode = cjson.encode
local tinsert = table.insert

--[[local dbg= function(...)
  local out = {...}
  for i,v in pairs(out) do
    out[i]=tostring(v)
  end
  redis.call("ECHO", table.concat(out, "    "))
end
]]
local Jsonobj; do
  local Jmeta = {__index = {
    use = function(self, ...)
      for _, k in ipairs {...} do
        local val
        if type(k) == "table" then
          val = k[2](self.data[k[1]])
          k = k[1]
        else
          val = self.data[k]
        end
        self:add(k, val)
      end
    end,
    useRaw = function(self, ...)
      for _, k in ipairs {...} do
        self:addRaw(k, self.data[k])
      end
    end,
    add = function(self, key, data, raw)
      if data == nil then return end
      if not raw then data = encode(data) end
      tinsert(self.buf, ("%s: %s"):format(encode(key), data))
    end,
    addRaw = function(self, key, data)
      return self:add(key, data, true)
    end,
    json = function(self)
      return ("{ %s }"):format(table.concat(self.buf, ", \n"))
    end
  }}
  Jsonobj = function(data)
    return setmetatable({buf={}, data=data or {}}, Jmeta)
  end
end

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

local function as_json(what, name)
  local j, ll
  if what == "rule" then
    j = Jsonobj(redis_gethash(keyf.rule:format(name)))
    if not j then return end
    j:use("name","info",{"gen", tonumber}, "key")
    j:useRaw("if", "then", "else")

  elseif what == "limiter" then
    j = Jsonobj(redis_gethash(keyf.limiter:format(name)))
    if not j then return end
    j:use("name","info", {"gen", tonumber}, {"interval", tonumber}, {"limit", tonumber}, {"sync-steps", tonumber}, "burst", {"burst-expire", tonumber})
    
  elseif what == "list" then
    j = Jsonobj(redis_gethash(keyf.list:format(name)))
    if not j then return end
    j:use("name","info",{"gen", tonumber})
    ll = redis.call("LRANGE", keyf.list_rules:format(name), 0, -1)
    if ll and #ll > 0 then
      j:add("rules", ll)
    else
      j:addRaw("rules", "[]")
    end
  
  elseif what == "phase" then
    ll = redis.call("LRANGE", keyf.phase_lists:format(name), 0, -1)
    return #ll > 0 and encode(ll) or "[]"

  elseif what == "ruleset" then
    j = Jsonobj(redis_gethash(key.ruleset))
    if not j then return end
    
    local function add_ruleset_items(the_item, items_set_key, attribute)
      local jj = Jsonobj()
      local members = redis.call("SMEMBERS", items_set_key)
      table.sort(members)
      for _, n in ipairs(members) do
        jj:addRaw(n, as_json(the_item, n))
      end
      j:addRaw(attribute, jj:json())
    end
    
    add_ruleset_items("limiter", key.limiters, "limiters")
    add_ruleset_items("phase", key.phases, "phases")
    add_ruleset_items("list", key.lists, "lists")
    add_ruleset_items("rule", key.rules, "rules")
  else
    error(("unknown ruleset item \"%s\""):format(what))
  end
  return j:json()
end

return as_json(item, item_name)
