local mm = require "mm"
local Redis = require "redis"

--luacheck: globals registerRedis connectRedises

local function parseRedisUrl(url)
  local host, port, pass, db
  local rest
  
  mm(url)
  
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
  mm(ret)
  
  return ret
end

local redises = {}
  
function registerRedis(url)
  local exists = redises[url]
  if exists then return exists end
  
  local parsedUrl = parseRedisUrl(url)
  
  local r = Redis.new(parsedUrl.host, parsedUrl.port, parsedUrl.pass, parsedUrl.db)
  redises[parsedUrl.url]=r
  mm(redises)
  return parsedUrl.url
end

function connectRedises()
  for _, r in pairs(redises) do
    r:connect()
  end
end

return function(redis_connect, redis_close, redis_command, loadscripts, timeout, get_hiredis_asyncContext_peername)
  assert(type(redis_connect) == "function")
  assert(type(redis_command) == "function")
  assert(type(loadscripts) == "function")
  assert(type(timeout) == "function")
  Redis.c.redis_connect = redis_connect
  Redis.c.redis_close = redis_close
  Redis.c.redis_command = redis_command
  Redis.c.loadscripts = loadscripts
  Redis.c.timeout = timeout
  Redis.c.get_hiredis_asyncContext_peername = get_hiredis_asyncContext_peername
end
