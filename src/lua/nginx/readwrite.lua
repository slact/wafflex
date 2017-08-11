--readwrite suspend/resume stuff

--luacheck: globals readwriteSuspendRead readwriteResumeRead readwriteSuspendWrite readwriteResumeWrite

local waiting_readers = {}
local waiting_writers = {}

return function(send_ipc_read_resume_alert, resume_write)
  function readwriteSuspendRead(rw, worker, evaldata)
    if not waiting_readers[rw] then
      waiting_readers[rw]={}
    end
    table.insert(waiting_readers[rw], {worker = worker, evaldata = evaldata})
  end
  
  function readwriteResumeRead(rw)
    if not waiting_readers[rw] then
      return
    end
    for _, v in ipairs(waiting_readers[rw]) do
      send_ipc_read_resume_alert(v.worker, v.evaldata)
    end
    waiting_readers[rw]=nil
  end
  
  function readwriteSuspendWrite(rw, update_callback, delta, ref)
    assert(waiting_writers[rw] == nil)
    waiting_writers[rw]={callback=update_callback, delta = delta, ref = ref}
  end
  
  function readwriteResumeWrite(rw)
    local waiting = waiting_writers[rw]
    waiting.callback(waiting.ref, waiting.delta)
  end
end
