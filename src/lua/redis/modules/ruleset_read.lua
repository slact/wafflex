
--luacheck: globals redis cjson

local encode = cjson.encode
local decode = cjson.decode
local tinsert = table.insert

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

local kbase, key, keyf, current_ruleset_name, current_prefix
local function genkeys(prefix, ruleset_name)
  if current_ruleset_name == ruleset_name and current_prefix == prefix then return end
  kbase = ("%sruleset:%s"):format(prefix, ruleset_name)
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
  current_prefix = prefix
  current_ruleset_name = ruleset_name
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

local function get_thing(prefix, ruleset_name, item, item_name, want_json, incomplete)
  genkeys(prefix, ruleset_name)
  local j
  if item == "rule" then
    local rule = redis_gethash(keyf.rule:format(item_name))
    if not rule then return nil end
    if want_json then
      j = Jsonobj(rule)
      j:use("name","info",{"gen", tonumber}, "key")
      j:useRaw("if", "then", "else")
      return j:json()
    else
      for _, k in ipairs{"if", "then", "else"} do
        if rule[k] then rule[k]=decode(rule[k]) end
      end
      rule.gen = tonumber(rule.gen)
      return rule
    end
  elseif item == "limiter" then
    local lim = redis_gethash(keyf.limiter:format(item_name))
    if not lim then return nil end
    if want_json then
      j = Jsonobj(lim)
      j:use("name","info", {"gen", tonumber}, {"interval", tonumber}, {"limit", tonumber}, {"sync-steps", tonumber}, "burst", {"burst-expire", tonumber})
      return j:json()
    else
      lim.gen = tonumber(lim.gen)
      lim.interval = tonumber(lim.interval)
      lim.limit = tonumber(lim.limit)
      lim["sync-steps"] = tonumber(lim["sync-steps"])
      lim["burst-expire"] = tonumber(lim["burst-expire"])
      return lim
    end
  elseif item == "list" then
    local list = redis_gethash(keyf.list:format(item_name))
    if not list then return nil end
    if want_json then
      j = Jsonobj(list)
      j:use("name","info",{"gen", tonumber})
      local ll = redis.call("LRANGE", keyf.list_rules:format(item_name), 0, -1)
      if ll and #ll > 0 then
        j:add("rules", ll)
      else
        j:addRaw("rules", "[]")
      end
      return j:json()
    else
      list.gen = tonumber(list.gen)
      if not incomplete then
        list.rules = redis.call("LRANGE", keyf.list_rules:format(item_name), 0, -1)
      else
        list.incomplete = true
        list.external = true
      end
      return list
    end
  elseif item == "phase" then
    local ll = redis.call("LRANGE", keyf.phase_lists:format(item_name), 0, -1)
    if want_json then
      return #ll > 0 and encode(ll) or "[]"
    else
      return ll
    end
  elseif item == "ruleset" then
    local rs = redis_gethash(key.ruleset)
    if not rs then return nil end
    
    rs.gen = tonumber(rs.gen)
    
    local function add_ruleset_items(the_item, items_set_key, attribute)
      local member_names = redis.call("SMEMBERS", items_set_key)
      
      if want_json then
        local jj = Jsonobj()
        table.sort(member_names)
        for _, n in ipairs(member_names) do
          jj:addRaw(n, get_thing(prefix, ruleset_name, the_item, n, want_json, incomplete))
        end
        j:addRaw(attribute, jj:json())
      else
        local members = {}
        for _, n in ipairs(member_names) do
          members[n]=get_thing(prefix, ruleset_name, the_item, n, want_json, incomplete)
        end
        rs[attribute]=members
      end
    end
    
    if want_json then
      j = Jsonobj(rs)
      j:use("name","info",{"gen", tonumber})
    end
      
    add_ruleset_items("limiter", key.limiters, "limiters")
    add_ruleset_items("phase", key.phases, "phases")
    add_ruleset_items("list", key.lists, "lists")
    add_ruleset_items("rule", key.rules, "rules")
      
    return want_json and j:json() or rs
    
  else
    error("unknown thing we want here: " .. tostring(item))
  end
end

return get_thing
