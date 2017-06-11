
--luacheck: globals redis cjson ARGV
local prefix =        ARGV[1]
local item =          ARGV[2]
local ruleset_name =  ARGV[3]
local item_name =     ARGV[4]


local encode = cjson.encode
local tinsert = table.insert

local Jsonobj; do
  local Jmeta = {__index = {
    use = function(self, ...)
      for _, k in ipairs {...} do
        self:add(k, self.data[k])
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
      return ("{ %s }"):format(table.concat(self.buf, ", "))
    end
  }}
  Jsonobj = function(data)
    return setmetatable({buf={}, data=data or {}}, Jmeta)
  end
end

local kbase = ("%sruleset:%s"):format(prefix and prefix..":" or "", ruleset_name)
local key = {
  ruleset =  kbase,
  phases =   kbase..":phases",
  lists =    kbase..":lists",
  rules =    kbase..":rules",
  limiters = kbase..":limiters"
}

local keyf = {
  list = kbase..":list:%s",
  list_rules = kbase..":list:%s:rules",
  rule = kbase..":rule:%s",
  limiter = kbase..":limiter:%s",
  phase = kbase..":phase:%s"
}

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
  if item == "rule" then
    j = Jsonobj(redis_gethash(keyf.rule:format(name)))
    if not j then return end
    j:use("name","info","gen", "key")
    j:useRaw("if", "then", "else")

  elseif item == "limiter" then
    j = Jsonobj(redis_gethash(keyf.limiter:format(name)))
    if not j then return end
    j:use("name","info","gen","interval","limit","sync-steps","burst","burst-expire")
    
  elseif item == "list" then
    j = Jsonobj(redis_gethash(keyf.list:format(name)))
    if not j then return end
    j:use("name","info","gen")
    ll = redis.command("LRANGE", keyf.list_rules:format(name), 0, -1)
    if ll then
      j:add("rules", ll)
    else
      j:addRaw("rules", "[]")
    end
  
  elseif item == "phase" then
    ll = redis.command("LRANGE", keyf.phase:format(name), 0, -1)
    return ll and encode(ll) or "[]"

  elseif item == "ruleset" then
    j = Jsonobj(redis_gethash(key.ruleset))
    if not j then return end
    
    local function add_ruleset_items(the_item, items_set_key, attribute)
      local jj = Jsonobj()
      for _, n in ipairs(table.sort(redis.comand("SMEMBERS", items_set_key))) do
        jj:addRaw(n, as_json(the_item, n))
      end
      j:addRaw(attribute, jj:json())
    end
    
    add_ruleset_items("limiter", key.limiters, "limiters")
    add_ruleset_items("phase", key.phases, "phases")
    add_ruleset_items("list", key.lists, "lists")
    add_ruleset_items("rule", key.rules, "rules")
  else
    error(("unknown ruleset item \"%s\""):format(item))
  end
  return j:json()
end

return as_json(item, item_name)
