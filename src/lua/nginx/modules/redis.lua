local module = {c={}}
local mm = require "mm"

--luacheck: globals newRedis

local Redis = {}
local rmeta = {__index = Redis}
local script = {}
local script_hash = {}

function Redis:connect()
  mm(self)
  assert(self.status == "disconnected", "should only try to connect when disconnected")
  
  local function connector()
    local ret, err = module.c.redis_connect(self.connection_params.host, self.connection_params.port)
    if not ret then
      print("OH NO THAT DIDN'T WORK: " .. tostring(err))
      --TODO: errorstuff
      return self:reconnect(10)
    end
    assert(type(ret) == "userdata")
    self.ctx = ret
    
    self.connection_params.peername = module.c.get_hiredis_asyncContext_peername(self.ctx)
    ret, err = module.c.redis_connect(self.connection_params.peername, self.connection_params.port)
    if not ret then
      print("OH NO THAT DIDN'T WORK: " .. tostring(err))
      --TODO: errorstuff
      return self:reconnect(10)
    end
    assert(type(ret) == "userdata")
    self.sub_ctx = ret

    if self.connection_params.pass then
      self:ignore_next_status_check()
      ret, err = self:command("AUTH", self.connection_params.pass)
      if not ret then
        --TODO: error, wrong password
        print("OH NO THAT DIDN'T WORK: " .. tostring(err))
        return self:reconnect(10)
      end
      
      self:ignore_next_status_check()
      ret, err = self:sub_command("AUTH", self.connection_params.pass)
      if not ret then
        --TODO: error, wrong password
        print("OH NO THAT DIDN'T WORK: " .. tostring(err))
        return self:reconnect(10)
      end
    end
    
    if self.connection_params.db then
      self:ignore_next_status_check()
      ret, err = self:command("SELECT", self.connection_params.db)
      if not ret then
        --TODO: error, wrong password
        print("OH NO THAT DIDN'T WORK: " .. tostring(err))
        return self:reconnect(10)
      end
      
      self:ignore_next_status_check()
      ret, err = self:sub_command("SELECT", self.connection_params.db)
      if not ret then
        --TODO: error, wrong password
        print("OH NO THAT DIDN'T WORK: " .. tostring(err))
        return self:reconnect(10)
      end
    end
    
    return self
  end
  
  if coroutine.running() then
    --use current coroutine
    return connector()
  else
    --new coroutine plz
    return (coroutine.wrap(connector))()
  end
end

local function subscribe_handler(data)
  print("GOT SUBSCRIBE THINGY")
  mm(data)
end

function Redis:subscribe(channel, callback)
  assert(self.pubsub_handlers[channel] == nil, ("already subscribed to \"%s\""):format(channel))
  assert(type(callback) == "function")
  assert(type(channel) == "string")
  self.pubsub_handlers[channel]=callback
  self:sub_command(subscribe_handler, "SUBSCRIBE", channel)
end

function Redis:unsubscribe(channel)
  self.pubsub_handlers[channel]=nil
end

function Redis:script(name, keys, ...)
  return self:command("evalsha", #keys, table.unpack(keys), ...)
end

local function infer_command_callback_and_args(first, ...)
  if type(first) == "function" then
    return first, ...
  else
    local co = coroutine.running()
    assert(co, "no callback given to redis command, expected to be run in coroutine")
    local coroutine_resumer = function(...)
      coroutine.resume(co, ...)
    end
    return coroutine_resumer, first, ...
  end
end

local function redis_command(self, ctx_name, ...)
  if not self:check_status_ready() then
    table.insert(self.pending_commands[ctx_name], {infer_command_callback_and_args(...)})
    return true
  else
    return module.c.redis_command(self[ctx_name], infer_command_callback_and_args(...))
  end
end

function Redis:check_status_ready()
  if self.ignore_check_status > 0 then
    self.ignore_check_status = self.ignore_check_status - 1
    return true
  else
    return self.status == "ready"
  end
end

function Redis:ignore_next_status_check()
  self.ignore_check_status = self.ignore_check_status + 1
end

function Redis:command(...)
  return redis_command(self, "ctx", ...)
end
function Redis:sub_command(...)
  return redis_command(self, "sub_ctx", ...)
end

function Redis:script(name, first, ...)
  local myhash = script_hash[name]
  assert(myhash, ("unknown Redis lua script \"%s\""):format(name))
  if type(first) == "table" then
    return self:command("EVALSHA", myhash, #first, table.unpack(first), ...)
  else
    return self:command("EVALSHA", myhash, 0, first, ...)
  end
end

function newRedis(host, port, pass, db)
  local r = setmetatable({}, rmeta)
  r.connection_params = {
    host=host, port=port, pass=pass, db=db
  }
  
  r.pubsub_handlers = {}
  r.subscribed = false
  
  r.ignore_check_status = 0
  r.status = "disconnected"
  r.pending_commands={ctx={}, sub_ctx={}}
  
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
