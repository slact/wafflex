local module = {c={}}
--local mm = require "mm"
--luacheck: globals newRedis

local Redis = {}
local rmeta = {__index = Redis}
local scripts = {}
local script_hash = {}

local table_rip = function(tbl)
  local keys = {}
  local vals = {}
  for k, v in pairs(tbl) do
    table.insert(keys, k)
    table.insert(vals, v)
  end
  return keys, vals
end

local function current_coroutine()
  local co, main = coroutine.running()
  if co and main == true then
    co = nil
  end
  return co
end

local function error_catching_command_handler(res, err)
  assert(res, err)
end

local function infer_command_callback_and_args(first, ...)
  local t = type(first)
  if t == "function" or t == "thread" then
    return first, ...
  else
    --compatible with lua 5.1 and >=5.2, which have slightly different coroutine,running() return values
    local co = current_coroutine()
    
    if co == nil then
      --no callback, run it with default error-catcher
      return error_catching_command_handler, first, ...
    end
    
    --assert(co, "no callback given to redis command, expected to be run in coroutine")
    return co, first, ...
  end
end

local function raw_redis_command(ctx, ...)
  return module.c.redis_command(ctx, infer_command_callback_and_args(...))
end

local function redis_initialize_connection(self, ctx)
  local ret, err
  if self.connection_params.pass then
    ret, err = raw_redis_command(ctx, "AUTH", self.connection_params.pass)
    if not ret then
      --TODO: error, wrong password
      return nil, err
    end
  end
  
  if self.connection_params.db then
    ret, err = raw_redis_command(ctx, "SELECT", self.connection_params.db)
    if not ret then
      --TODO: error, wrong password
      return nil, err
    end
  end
  
  return ctx
end

function Redis:connect()
  assert(self:checkStatus("disconnected"), "should only try to connect when disconnected")
  assert(self.ctx == nil)
  assert(self.sub_ctx == nil)
  
  self:setStatus("connecting")
  local err
  self.ctx, err = module.c.redis_connect(self.connection_params.host, self.connection_params.port) --synchronous
  if not self.ctx then
    error(err)
  end
  self.connection_params.peername = module.c.get_hiredis_asyncContext_peername(self.ctx)
  
  self.sub_ctx, err = module.c.redis_connect(self.connection_params.peername, self.connection_params.port) --synchronous
  if not self.sub_ctx then
    error(err)
  end
  
  local function fail(msg)
    self:setStatus("disconnected")
    if self.ctx then
      module.c.redis_close(self.ctx)
      self.ctx = nil
    end
    if self.sub_ctx then
      module.c.redis_close(self.sub_ctx)
      self.sub_ctx = nil
    end
    
    module.c.log_error("error", ("Failed to connect to Redis server: %s. Retry in 5 sec."):format(msg or "unknown error"))
    
    module.c.timeout(5000, coroutine.wrap(function()
      self:connect()
    end))
    return nil
  end
  
  
  local function get_info(info, what)
    local m = "^"..what..":(.*)"
    local ret
    for line in info:gmatch('[^\r\n]+') do
      ret = line:match(m)
      if ret then return ret end
    end
    return nil
  end
  local function connector()
    --luacheck: push ignore 431 --don't mind the shadowing
    if not redis_initialize_connection(self, self.ctx) then
      return fail("error connecting command client")
    end
    if not redis_initialize_connection(self, self.sub_ctx) then
      return fail("error connecting pubsub client")
    end
    
    local res, info, err
    
    info, err = raw_redis_command(self.ctx, "INFO")
    if not info then return fail(err) end
    local loading = get_info(info, "loading") == "1"
    
    while loading do
      --wait until finished loading
      module.c.timeout(1000)
      info, err = raw_redis_command(self.ctx, "INFO")
      if not info then return fail(err) end
      loading = get_info(info, "loading") == "1"
    end
    
    local cluster_enabled = get_info(info, "cluster_enabled") == "1"
    if cluster_enabled then
      return fail("can't handle clusters yet")
    end
    
    local role = get_info(info, "role")
    if role ~= "master" then
      return fail("useless to connect to a slave (for now)")
    end
    
    --load scripts
    local check_script_names, check_script_hashes = table_rip(script_hash)
    local script_existence
    if #check_script_names > 0 then
    
      script_existence, err = raw_redis_command(self.ctx, "SCRIPT", "EXISTS", table.unpack(check_script_hashes))
      if not script_existence then return fail(err) end
      
      for i, name in ipairs(check_script_names) do
        if script_existence[i] == 0 then --script does not exist
          local hash = script_hash[name]
          
          res, err = raw_redis_command(self.ctx, "SCRIPT", "LOAD", scripts[name])
          if not res then return fail(("error in script %s\n%s"):format(name, err)) end
          if hash ~= res then return fail("loaded script hash doesn't match") end
          
        end
      end
      
      --loaded the scripts. I think that's everything...
    end
    
    self:setStatus("ready")
    --luacheck: pop
  end
  
  
  if current_coroutine() then
    return connector()
  else
    return (coroutine.wrap(connector))()
  end
end

function Redis:subscribe(channel, callback)
  
  if not self.__subscribe_handler then
    self.__subscribe_handler = function(data, err)
      if not data then
        error(err)
      end
      local kind = data[1]
      if kind =="message" then
        local message_handler = self.pubsub_handlers[data[2]]
        if message_handler then
          message_handler(data[3], data[2])
        end
      elseif kind == "unsubscribe" then
        self.pubsub_handlers[data[2]] = nil
        if not next(self.pubsub_handlers) then
          self.__subscribe_handler = nil
        end
      end
      return self.__subscribe_handler
    end
  end
  
  assert(self.pubsub_handlers[channel] == nil, ("already subscribed to \"%s\""):format(channel))
  assert(type(callback) == "function")
  assert(type(channel) == "string")
  self.pubsub_handlers[channel]=callback
  self:sub_command(self.__subscribe_handler, "SUBSCRIBE", channel)
end

function Redis:unsubscribe(channel)
  if not self.pubsub_handlers[channel] then
    return nil, "not subscribed to " .. tostring(channel)
  end
  return self:sub_command(self.__subscribe_handler, "unsubscribe", channel)
end

local function redis_command(self, ctx_name, ...)
  if self:checkStatus("ready") then
    return raw_redis_command(self[ctx_name], infer_command_callback_and_args(...))
  else
    local args = {infer_command_callback_and_args(...)}
    if type(args[1] == "thread") then
      --we're in a coroutine
      table.insert(self.pending_commands, {coroutine=args[1]})
      assert(coroutine.yield() == "ready now") --wait until resumed when ready
      return redis_command(self, ctx_name, ...)
    else
      table.insert(self.pending_commands, {ctx_name = ctx_name, args=args})
    end
  end
end


local legit_status = {
  disconnected = 1,
  connecting = 2,
  authenticating = 3,
  ready = 4
}
function Redis:getStatus()
  return self.__status
end
function Redis:checkStatus(compare)
  if not legit_status[compare] then
    error("invalid status " .. tostring(compare))
  end
  return (self.__status or "disconnected") == compare
end
function Redis:setStatus(status)
  if not legit_status[status] then
    error("invalid status " .. tostring(status))
  end
  local prev_status = self.__status
  self.__status = status
  
  if status == "ready" and prev_status ~= "ready" then
    for _, cmd in ipairs(self.pending_commands) do
      if cmd.coroutine then
        coroutine.resume(cmd.coroutine, "ready now")
      else
        redis_command(self, cmd.ctx_name, table.unpack(cmd.args))
      end
    end
  end
  
  return true
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

function Redis:script(name, ...)
  local myhash = script_hash[name]
  local args = {infer_command_callback_and_args(...)}
  assert(myhash, ("unknown Redis lua script \"%s\""):format(name))

  
  if type(args[2]) == "table" then
    local keys = args[2]
    table.remove(args, 2)
    table.insert(args, 2, #keys)
    for i, v in ipairs(keys) do
      table.insert(args, 2+i, v)
    end
  else
    table.insert(args, 2, 0)
  end
  table.insert(args, 2, "EVALSHA")
  table.insert(args, 3, myhash)
  return self:command(table.unpack(args))
end

function newRedis(host, port, pass, db)
  local r = setmetatable({}, rmeta)
  r.connection_params = {
    host=host, port=port, pass=pass, db=db
  }
  
  r.pubsub_handlers = {}
  r.subscribed = false
  
  r.pending_commands = {}
  
  return r
end

module.new = newRedis

function module.addScript(name, hash, src)
  assert(scripts[name] == nil, ("redis lua script \"%s\" already exists"):format(name))
  script_hash[name]=hash
  scripts[name]=src
end

for _,v in pairs {"redis_connect", "redis_close", "redis_command", "loadscripts", "timeout", "log_error"} do
  module.c[v]=function()
    error("c binding " .. v .. " not set")
  end
end

return module
