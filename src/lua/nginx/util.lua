local reqs = {}
--luacheck: globals trackPtr getTrackedPtr untrackPtr
function trackPtr(request_ptr, data)
  local first_time = reqs[request_ptr] == nil
  reqs[request_ptr] = true
  return first_time
end

function getTrackedPtr(request_ptr)
  return reqs[request_ptr]
end

function untrackPtr(request_ptr)
  reqs[request_ptr]=nil
end
