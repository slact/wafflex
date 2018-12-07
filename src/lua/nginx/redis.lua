local Redis = require "redis"
--local mm = require "mm"
local json = require "dkjson"
local Parser = require "parser"
local mm = require "mm"

--luacheck: globals registerRedis connectRedises testRedisConnector findRedis getRedisRulesetJSON redisRulesetSubscribe redisRulesetUnsubscribe findRuleset wafflexSubscribeHandler TracerRound ngx

local function parseRedisUrl(url)
  local host, port, pass, db
  local rest
  
  url = url:match("^redis://(.*)") or url
  pass, rest = url:match("^:([^@]+)@(.*)")
  if pass then url = rest end
  host, rest = url:match("^([^:/]+)(.*)")
  if host then url = rest end
  port, rest = url:match("^:(%d+)(.*)")
  if port then port=tonumber(port); url = rest end
  db = url:match("^/(%d+)")
  if db then db = tonumber(db) end
  
  local ret = {
    url=("redis://%s%s:%i%s"):format(pass and ":"..pass.."@" or "", host, port, db and "/"..db or ""),
    host = host,
    port = port,
    pass = pass,
    db = db
  }
  
  return ret
end

local redises = {}
local redis_conf = {}

function findRedis(conf_ptr)
  return redis_conf[conf_ptr]
end

function registerRedis(url, conf_ptr)
  local exists = redises[url]
  if exists then
    table.insert(exists.conf_ptrs, conf_ptr)
    redis_conf[conf_ptr]=exists
    return exists
  end
  
  local parsedUrl = parseRedisUrl(url)
  
  local r = Redis.new(parsedUrl.host, parsedUrl.port, parsedUrl.pass, parsedUrl.db)
  redises[parsedUrl.url]=r
  
  if not r.conf_ptrs then
    r.conf_ptrs = {}
  end
  table.insert(r.conf_ptrs, conf_ptr)
  redis_conf[conf_ptr]=r
  return parsedUrl.url
end

function wafflexSubscribeHandler(redis, data)
  local msg = json.decode(data, 1, json.null, nil)
  if not msg then
    ngx.log("error", "invalid non-json mesage: \"" .. data .. "\"")
    return
  end
  local cmd = msg.cmd
  local ok, err
  if     cmd == "load-tracer-round" then
    ok, err = TracerRound.create(msg.data)
  elseif cmd == "unload-tracer-round" then
    ok, err = TracerRound.delete(msg.data)
  elseif cmd == "stats" then
    ok, err = nil, "stats command not implemented"
  else
    error("received unknown command \"" .. tostring(msg.cmd) .. "\"")
  end
  
  if not ok then
    ngx.log("error", err)
  end
  
end

function connectRedises()
  for _, r in pairs(redises) do
    r:connect()
    r:subscribe("wafflex", function(data, channel)
      mm("yeap")
      wafflexSubscribeHandler(r, data)
    end)
  end
end

function getRedisRulesetJSON(conf_ptr, ruleset_name)
  local redis = findRedis(conf_ptr)
  if not redis then return nil, "no redis found for conf_ptr " .. tostring(conf_ptr) end
  local ruleset_json, err  = redis:script("ruleset_read", "", ruleset_name,  "ruleset")
  if ruleset_json == 0 then
    return nil, err
  else
    return ruleset_json
  end
end

function redisRulesetSubscribe(conf_ptr, ruleset_name)
  local redis = findRedis(conf_ptr)
  if not redis then return nil, "no redis found for conf_ptr " .. tostring(conf_ptr) end
  --TODO: prefix
  
  local updater = function(data)
    local parser = Parser.new()
    local ruleset = findRuleset(ruleset_name)
    if not ruleset then
      error("can't find ruleset " .. ruleset_name)
    end
    local msg = json.decode(data, 1, json.null, nil)
    if msg.action == "create" then
      --create stuff
      if msg.type == "rule" then
        local rule = parser:parseJSON("rule", msg.data)
        rule.name = msg.name
        ruleset:addRule(rule)
      end
    elseif msg.action == "update" then
      if msg.type == "rule" then
        local rule = parser:parseJSON("rule", msg.data)
        rule.name = msg.name
        ruleset:updateRule(rule)
      else
        error("don't know what to do")
      end
    else
      error("don't know what action this is")
    end
  end
  
  return redis:subscribe(("ruleset:%s:pubsub"):format(ruleset_name), updater)
end
function redisRulesetUnsubscribe(conf_ptr, ruleset_name)
  local redis = findRedis(conf_ptr)
  if not redis then return nil, "no redis found for conf_ptr " .. tostring(conf_ptr) end
  return redis:unsubscribe(("ruleset:%s:pubsub"):format(ruleset_name))
end

function testRedisConnector()
  local r = Redis.new("localhost", 8537, nil, 2)
  coroutine.wrap(function()
    print(r:connect())
    
    local tend = os.clock() + 3
    while os.clock()<tend do
    --nothing
    end
    
    print(r:script("init"))
    
    r:subscribe("channel2", function(msg, channel)
      print("SUBSCRIBE2 GOT")
      r:unsubscribe("channel1")
    end)
    
    r:subscribe("channel1", function(msg, channel)
      print("SUBSCRIBE GOT")
      print(msg, channel)
    end)
    
    print("OKAY THEN")
  end)()
end

return function(redis_connect, redis_close, redis_command, get_hiredis_asyncContext_peername, timeout, scripts)
  assert(type(redis_connect) == "function")
  assert(type(redis_command) == "function")
  assert(type(timeout) == "function")
  Redis.c.redis_connect = redis_connect
  Redis.c.redis_close = redis_close
  Redis.c.redis_command = redis_command
  Redis.c.timeout = timeout
  Redis.c.get_hiredis_asyncContext_peername = get_hiredis_asyncContext_peername
  for _, script in ipairs(scripts) do
    Redis.addScript(script.name, script.hash, script.src)
  end
end
