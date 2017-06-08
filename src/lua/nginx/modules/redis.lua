local module = {c={}}
local mm = require "mm"

local Redis = {}
local rmeta = {__index = Redis}
local script = {}
local script_hash = {}

function Redis:connect()
  mm(self)
  self._connector = coroutine.wrap(function()
    local ret, err = module.c.redis_connect(self.connection_params.host, self.connection_params.port)
    if not ret then
      print("OH NO THAT DIDN'T WORK")
      --TODO: errorstuff
      return self:reconnect(10)
    end
    assert(type(ret) == "userdata")
    self.ctx = ret
    if self.connection_params.pass then
      ret, err = self:command("AUTH", self.connection_params.pass)
      if not ret then
        --TODO: error, wrong password
        return self:reconnect(10)
      end
    end
    
    print("WEEEEEE")
    mm(self)
    
    if self.connection_params.db then
      ret, err = self:command("SELECT", self.connection_params.db)
      print(ret, err)
      if not ret then
        --TODO: error, wrong password
        return self:reconnect(10)
      end
    end
    return self
  end)
  return self._connector()
end

function Redis:subscribe(channel, callback)
  assert(self.pubsub_handlers[channel] == nil, ("already subscribed to \"%s\""):format(channel))
  assert(type(callback) == "function")
  self.pubsub_handlers[channel]=callback
end

function Redis:unsubscribe(channel)
  self.pubsub_handlers[channel]=nil
end

function Redis:script(name, keys, ...)
  return self:command("evalsha", #keys, table.unpack(keys), ...)
end

function Redis:command(first, ...)
  print("COMMAND", first, ...)
  if type(first) == "function" then
    return module.c.redis_command(self.ctx, first, ...)
  else
    local co = coroutine.running()
    assert(co, "no callback given to redis command, expected to be run in coroutine")
    local resumer = function(...)
      coroutine.resume(co, ...)
    end
    
    module.c.redis_command(self.ctx, resumer, first, ...)
    return coroutine.yield()
  end
end

function newRedis(host, port, pass, db)
  local r = setmetatable({}, rmeta)
  r.connection_params = {
    host=host, port=port, pass=pass, db=db
  }
  
  r.pubsub_handlers = {}
  r.subscribed = false
  
  return r
end

module.new = newRedis

function module.addScript(name, hash, src)
  assert(script[name] == nil, ("redis lua script \"%s\" already exists"):format(name))
  script_hash[name]=hash
  script[name]=src
end

for _,v in pairs {"redis_connect", "redis_close", "redis_command", "loadscripts", "timeout"} do
  module.c[v]=function()
    error("c binding " .. v .. " not set")
  end
end

return module
