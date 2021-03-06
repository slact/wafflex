local mm = require "mm"
--luacheck: globals Tracer TracerRound ngx
local tracers = {}
local RuleComponent = require "rulecomponent"
local Binding = require "binding"

Tracer = {}

local function tracerCleaner(ref)
  print("CLEANED UP AFTER TRACER")
  tracers[ref] = nil
end

local tracer_round_mt = { __index = {
}}

TracerRound = {
  create = function(data)
    local round = {
      id = data.id,
      condition = RuleComponent.condition.new(data.condition, nil),
      target = data.target,
      target_type = data.target_type,
      target_key = data.target_key,
      profile = data.profile,
      trace = data.trace,
      uses = data.uses
    }
    setmetatable(round, tracer_round_mt)
    Binding.call("tracer-round", "create", round)
    --TODO: error check binding creation
    return round
  end,
  
  delete = function(round)
    if round.condition then
      RuleComponent.condition.delete(round.condition, round)
    end
    Binding.call("tracer-round", "create", round)
  end,
  
  find = nil --will be filled in whenscript is called
}

local tracer_mt = {__index = {
  push = function(self, element, el_name, el_gen, el_ref)
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
        el.time.start = ngx.cached_msec_time()
      end
      table.insert(self.stack, self.cur, el)
    else
      el = self.stack[self.cur]
      self.defer = self.defer - 1
      assert(el.deferred)
      --assert(el.ref == el_ref)
      el.deferred = nil
      if el.time.defer_start then
        el.time.defer = (el.time.defer or 0) + (ngx.cached_msec_time() - el.time.defer_start)
        el.time.defer_start = nil
      end
    end
  end,
  
  pop = function(self, element, rc, ...)
    local el
    print("popping", element, rc)
    if rc == "defer" then
      self.defer = self.defer + 1
      el = self.stack[self.cur]
      assert(el.type == element)
      el.deferred = true
      if self.profile then
        el.time.defer_start = ngx.cached_msec_time()
      end
      if el.time.start then
        el.time.run = el.time.run or 0 + (ngx.cached_msec_time() - el.time.start)
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
        el.time.run = el.time.run or 0 + (ngx.cached_msec_time() - el.time.start)
        el.time.total = el.time.run + (el.time.defer or 0)
        el.time.start = nil
      end
      table.remove(self.stack, self.cur)
      self:completeElement(el);
    end
    self.cur = self.cur - 1
  end,
  
  log = function(self, data_name, data)
    local el = self.stack[self.cur]
    if not el.data then el.data = {} end
    el.data[data_name]=data
  end,
  
  log_array = function(self, data_name, data)
    local el = self.stack[self.cur]
    if not el.data then el.data = {} end
    if not el.data[data_name] then
      el.data[data_name] = {}
    end
    table.insert(el.data[data_name], data)
  end,
  
  getTop = function(self, element)
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
  end,
  
  completeElement = function(self, el)
    local parent_type = {
      condition=  "rule",
      action=     "rule",
      rule=       "list",
      list=       "phase",
      phase=      "ruleset",
      ruleset=    "root"
    }
    local parent = self:getTop(parent_type[el.type])
    if not parent[el.type] then parent[el.type]={} end
    table.insert(parent[el.type], el)
  end,
  
  finish = function(self)
    if self.defer >  0 then
      return
    end
    mm(self)
  end
}}


function Tracer.create(ref_type, req_ref, opt)
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

function Tracer.get(ref_type, req_ref)
  local tracer =  tracers[req_ref] or Tracer.create(ref_type, req_ref)
  --mm(tracer)
  return tracer
end

return function(find_tracer_round_by_id)
  TracerRound.find = find_tracer_round_by_id
end

