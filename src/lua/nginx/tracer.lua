local mm = require "mm"
--luacheck: globals newTracer getTracer
local ngx = {}

local tracers = {}
  
local Tracer = {}
local tracer_mt = {__index = Tracer}

local function tracerCleaner(ref)
  print("CLEANED UP AFTER TRACER")
  tracers[ref] = nil
end

function newTracer(ref_type, req_ref)
  local tracer = {
    stack = {},
    complete = {},
    defer = 0,
    cur = 0,
    
    profile = true
  }
  setmetatable(tracer, tracer_mt)
  tracers[req_ref] = tracer
  if ref_type == "request" then
    ngx.add_request_cleanup_handler(req_ref, tracerCleaner)
  else
    error("don't know how to do this")
  end
  
  return tracer
end

function getTracer(ref_type, req_ref)
  local tracer =  tracers[req_ref] or newTracer(ref_type, req_ref)
  --mm(tracer)
  return tracer
end

function Tracer:push(element, el_name, el_gen, el_ref)
  print("pushing", element, el_name, "defer:", self.defer)
  self.cur = self.cur + 1
  local el
  if self.defer == 0 then
    el = {
      type = element,
      name = el_name,
      --gen = el_gen,
      --ref = el_ref,
      
      time = { }
    }
    if self.profile then
      el.time.start = ngx.time_msec()
    end
    table.insert(self.stack, self.cur, el)
  else
    el = self.stack[self.cur]
    self.defer = self.defer - 1
    assert(el.deferred)
    --assert(el.ref == el_ref)
    el.deferred = nil
    if el.time.defer_start then
      el.time.defer = (el.time.defer or 0) + (ngx.time_msec() - el.time.defer_start)
      el.time.defer_start = nil
    end
  end
end

function Tracer:pop(element, rc, ...)
  local el
  print("popping", element, rc)
  if rc == "defer" then
    self.defer = self.defer + 1
    el = self.stack[self.cur]
    assert(el.type == element)
    el.deferred = true
    if self.profile then
      el.time.defer_start = ngx.time_msec()
    end
    if el.time.start then
      el.time.run = el.time.run or 0 + (ngx.time_msec() - el.time.start)
      el.time.start = nil
    end
  else
    el = self.stack[self.cur]
    assert(el.type == element)
    el.result = rc
    if rc == "error" then
      el.error = ({...})[1]
    end
    if el.time.start then
      el.time.run = el.time.run or 0 + (ngx.time_msec() - el.time.start)
      el.time.total = el.time.run + (el.time.defer or 0)
      el.time.start = nil
    end
    table.remove(self.stack, self.cur)
    self:completeElement(el);
  end
  self.cur = self.cur - 1
end

function Tracer:log(data_name, data)
  local el = self.stack[self.cur]
  if not el.data then el.data = {} end
  el.data[data_name]=data
end

function Tracer:log_array(data_name, data)
  local el = self.stack[self.cur]
  if not el.data then el.data = {} end
  if not el.data[data_name] then
    el.data[data_name] = {}
  end
  table.insert(el.data[data_name], data)
end

local parent_type = {
  condition=  "rule",
  action=     "rule",
  rule=       "list",
  list=       "phase",
  phase=      "ruleset",
  ruleset=    "root"
}

function Tracer:getTop(element)
  mm(self.stack)
  if element == "root" then
    return self.complete
  end
  local cur
  for i = #self.stack, 1, -1 do
    cur = self.stack[i]
    if cur.type == element then
      return cur
    end
  end
  error("element type " .. element .. " not found in stack")
end

function Tracer:completeElement(el)
  local parent = self:getTop(parent_type[el.type])
  if not parent[el.type] then parent[el.type]={} end
  table.insert(parent[el.type], el)
end

function Tracer:finish()
  if self.defer >  0 then
    return
  end
  mm(self)
end

return function(ngx_cached_msec_time, ngx_cached_time, ngx_add_request_cleanup_handler, ngx_error_log)
  ngx.time_msec = function()
    local sec, msec = ngx_cached_msec_time()
    return sec + msec/1000
  end
  ngx.time_cached = ngx_cached_time
  ngx.error_log = ngx_error_log
  ngx.add_request_cleanup_handler = ngx_add_request_cleanup_handler
end
