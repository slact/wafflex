#include <nginx.h>
#include "reaper.h"
#include <assert.h>

//#define DEBUG_LEVEL NGX_LOG_WARN
#define DEBUG_LEVEL NGX_LOG_DEBUG
#define DBG(fmt, args...) ngx_log_error(DEBUG_LEVEL, ngx_cycle->log, 0, "REAPER: " fmt, ##args)
#define ERR(fmt, args...) ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "REAPER: " fmt, ##args)

static void reaper_timer_handler(ngx_event_t *ev);
void verify_reaper_list(reaper_t *rp, void *thing);

ngx_int_t reaper_start(reaper_t *rp, char *name, int prev, int next, ngx_int_t (*ready)(void *, void *, uint8_t), void (*reap)(void *, void *), void *pd, unsigned tick_usec) {
  rp->name = name;
  rp->count = 0;
  rp->next_ptr_offset = next;
  rp->prev_ptr_offset = prev;
  rp->last = NULL;
  rp->first = NULL;
  rp->ready = ready;
  rp->reap = reap;
  rp->privdata = pd;
  ngx_memzero(&rp->timer, sizeof(rp->timer));
#if nginx_version >= 1008000
  rp->timer.cancelable = 1;
#endif
  rp->timer.handler = reaper_timer_handler;
  rp->timer.data = rp;
  rp->timer.log = ngx_cycle->log;
  
  rp->tick_usec = tick_usec;
  rp->strategy = REAPER_RESCAN;
  rp->max_notready_ratio = 0; //disabled
  rp->position = NULL;
  
  DBG("start reaper %s with tick time of %i usec", name, tick_usec);
  verify_reaper_list(rp, NULL);
  return NGX_OK;
}

static void its_reaping_time(reaper_t *rp, uint8_t force);

ngx_int_t reaper_flush(reaper_t *rp) {
  its_reaping_time(rp, 1);
  return NGX_OK;
}

ngx_int_t reaper_stop(reaper_t *rp) {
  reaper_flush(rp);
  if(rp->timer.timer_set) {
    ngx_del_timer(&rp->timer);
  }
  DBG("stopped reaper %s", rp->name);
  return NGX_OK;
}

static ngx_inline void **thing_next_ptr(reaper_t *rp, void *thing) {
  return (void *)((char *)thing + rp->next_ptr_offset);
}
static ngx_inline void *thing_next(reaper_t *rp, void *thing) {
  return *thing_next_ptr(rp, thing);
}

static ngx_inline void **thing_prev_ptr(reaper_t *rp, void *thing) {
  return (void *)((char *)thing + rp->prev_ptr_offset);
}
static ngx_inline void *thing_prev(reaper_t *rp, void *thing) {
  return *thing_prev_ptr(rp, thing);
}

void reaper_each(reaper_t *rp, void (*cb)(void *thing, void *pd), void *pd) {
  void                *cur;
  for(cur = rp->first; cur != NULL; cur = thing_next(rp, cur)) {
    cb(cur, pd);
  }
}

ngx_inline void verify_reaper_list(reaper_t *rp, void *thing) {
  /*
  if(rp->first == NULL) {
    assert(rp->count == 0 && rp->last == NULL);
  }
  else {
    assert(rp->count > 0 && rp->last != NULL);
  }
  
  if(rp->last == NULL) {
    assert(rp->count == 0 && rp->first == NULL);
  }
  else {
    assert(rp->count > 0 && rp->first != NULL);
  }
  
  if(rp->count == 0) {
    assert(rp->last == NULL && rp->first == NULL);
  }
  else {
    assert(rp->last != NULL && rp->first != NULL);
  }
  
  int i=0;
  void *cur;
  for(cur = rp->first; cur != NULL; cur = thing_next(rp, cur)) {
    i++;
    if(thing) assert(cur != thing);
  }
  assert(i == rp->count);
  */
}

static ngx_inline void reaper_reset_timer(reaper_t *rp) {
  verify_reaper_list(rp, NULL);
  if (!ngx_exiting && !ngx_quit && rp->count > 0 && !rp->timer.timer_set) {
    DBG("reap %s again later (remaining: %i)", rp->name, rp->count);
    ngx_add_timer(&rp->timer, rp->tick_usec);
  }
  
}

ngx_int_t reaper_add(reaper_t *rp, void *thing) {
  verify_reaper_list(rp, thing);
  
  if(rp->ready(thing, rp->privdata, 0) == NGX_OK) {
    rp->reap(thing, rp->privdata);
    return NGX_DONE;
  }
  
  void    **next = thing_next_ptr(rp, thing);
  void    **prev = thing_prev_ptr(rp, thing);
  
  if(rp->last) {
    *thing_next_ptr(rp, rp->last) = thing;
  }
  *prev = rp->last;
  *next = NULL;
  rp->last = thing;
  
  if(rp->first == NULL) {
    rp->first = thing;
  }
  
  assert(rp->count >= 0);
  rp->count++;
  
  DBG("reap %s %p later (waiting to be reaped: %i)", rp->name, thing, rp->count);
  verify_reaper_list(rp, NULL);
  reaper_reset_timer(rp);
  return NGX_OK;
}


//this is not a safe function -- it doesn't check if the thing is actually being reaped.
ngx_int_t reaper_withdraw(reaper_t *rp, void *thing) {
  void *prev, *next;
  
  prev = thing_prev(rp, thing);
  next = thing_next(rp, thing);
  
  if(prev) *thing_next_ptr(rp, prev) = next;
  if(next) *thing_prev_ptr(rp, next) = prev;
  
  if(thing == rp->first) rp->first = next;
  if(thing == rp->last)  rp->last  = prev;
  
  assert(rp->count > 0);
  rp->count--;
  
  if(rp->strategy == REAPER_KEEP_PLACE && rp->position == thing) {
    rp->position = next;
  }
  
  *thing_next_ptr(rp, thing) = NULL;
  *thing_prev_ptr(rp, thing) = NULL;

  verify_reaper_list(rp, thing);
  
  DBG("withdraw %s %p", rp->name, thing);
  return NGX_OK;
}

static ngx_inline void reap_ready_thing(reaper_t *rp, void *cur, void *next) {
  void   *prev;
  prev = thing_prev(rp, cur);
  if(prev != NULL && next != NULL) {
    assert(prev != next);
  }
  assert(cur != prev);
  assert(cur != next);
  if(prev) *thing_next_ptr(rp, prev) = next;
  if(next) *thing_prev_ptr(rp, next) = prev;
  if(cur == rp->first) rp->first = next;
  if(cur == rp->last)  rp->last = prev;
  if(rp->strategy == REAPER_KEEP_PLACE && rp->position == cur) {
    rp->position = next;
  }
  rp->count--;
  
  rp->reap(cur, rp->privdata);
  
  assert(rp->count >= 0);
  verify_reaper_list(rp, cur);
  DBG("reaped %s %p (waiting to be reaped: %i)", rp->name, cur, rp->count);
}

static void its_reaping_time(reaper_t *rp, uint8_t force) {
  void                *cur = rp->first, *next;
  int                  max_notready, notready = 0; 
  void                *pd = rp->privdata;
  max_notready = rp->max_notready_ratio * rp->count;
  
  DBG("%s scan max notready %i", rp->name, max_notready);
  
  while(cur != NULL && notready <= max_notready) {
    next = thing_next(rp, cur);
    if(rp->ready(cur, pd, force) == NGX_OK) {
      reap_ready_thing(rp, cur, next);
      verify_reaper_list(rp, cur);
    }
    else if(max_notready > 0){
      DBG("not ready to reap %s %p", rp->name, cur);
      notready++;
      verify_reaper_list(rp, NULL);
    }
    cur = next;
  } 
}

static void its_reaping_time_keep_place(reaper_t *rp, uint8_t force) {
  void                *cur, *next;
  int                  max_notready, notready = 0; 
  int                  n = 0;
  void                *pd = rp->privdata;
  max_notready = rp->max_notready_ratio * rp->count;
  cur = rp->position == NULL ? rp->first : rp->position;
  
  DBG("%s keep_place max notready %i, cur %p", rp->name, max_notready, cur);
  
  while(n < rp->count && notready <= max_notready) {
    n++;
    next = thing_next(rp, cur);
    if(rp->ready(cur, pd, force) == NGX_OK) {
      reap_ready_thing(rp, cur, next);
      verify_reaper_list(rp, cur);
    }
    else if(max_notready > 0) {
      DBG("not ready to reap %s %p", rp->name, cur);
      notready++;
      verify_reaper_list(rp, NULL);
    }
    cur = (next == NULL ? rp->first : next);
  }
  rp->position = cur;
}

static void its_reaping_rotating_time(reaper_t *rp, uint8_t force) {
  void                *cur = rp->first, *next, *prev;
  void                *firstmoved = NULL;
  void               **next_ptr, **prev_ptr;
  int                  max_notready, notready = 0; 
  void                *pd = rp->privdata;
  
  max_notready = rp->max_notready_ratio * rp->count;
  
  DBG("%s rotatey max notready %i", rp->name, max_notready);
  
  while(cur != NULL && cur != firstmoved && notready <= max_notready) {
    next = thing_next(rp, cur);
    if(rp->ready(cur, pd, force) == NGX_OK) {
      reap_ready_thing(rp, cur, next);
    }
    else {
      //move to the end of the list
      //TODO: bulk move
      if(max_notready > 0) {
        DBG("not ready to reap %s %p", rp->name, cur);
        notready++;
        verify_reaper_list(rp, NULL);
      }
      
      if(!firstmoved) firstmoved = cur;
      
      if(rp->last != cur) {
        prev = thing_prev(rp, cur);
        
        if(prev) *thing_next_ptr(rp, prev) = next;
        if(next) *thing_prev_ptr(rp, next) = prev;
        
        next_ptr = thing_next_ptr(rp, cur);
        prev_ptr = thing_prev_ptr(rp, cur);
        
        if(rp->last) {
          *thing_next_ptr(rp, rp->last) = cur;
        }
        
        *prev_ptr = rp->last;
        *next_ptr = NULL;
        rp->last = cur;
        
        if(rp->first == NULL) {
          rp->first = cur;
        }
        else if(rp->first == cur) {
          rp->first = next;
        }
      }
      verify_reaper_list(rp, NULL);
    }
    cur = next;
  } 
}

static void reaper_timer_handler(ngx_event_t *ev) {
  reaper_t      *rp = ev->data;
  switch (rp->strategy) {
    case REAPER_RESCAN:
      its_reaping_time(rp, 0);
      reaper_reset_timer(rp);
      return;
    case REAPER_ROTATE:
      its_reaping_rotating_time(rp, 0);
      reaper_reset_timer(rp);
      return;
    case REAPER_KEEP_PLACE:
      its_reaping_time_keep_place(rp, 0);
      reaper_reset_timer(rp);
      return;
  }
}
