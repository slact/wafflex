#ifndef REAPER_H
#define REAPER_H

#include <nginx.h>
#include <ngx_http.h>

typedef enum {REAPER_RESCAN, REAPER_ROTATE, REAPER_KEEP_PLACE} reaper_strategy_t;

typedef struct {
  char                      *name;
  ngx_int_t                  count;
  int                        next_ptr_offset;
  int                        prev_ptr_offset;
  void                      *last;
  void                      *first;
  ngx_int_t                  (*ready)(void *item, void *pd, uint8_t force); //ready to be reaped?
  void                       (*reap)(void *item, void *pd); //reap it
  void                      *privdata;
  ngx_event_t                timer;
  int                        tick_usec;
  reaper_strategy_t          strategy;
  float                      max_notready_ratio;
  void                      *position;

} reaper_t;

ngx_int_t reaper_start(reaper_t *rp, char *name, int prev, int next, ngx_int_t (*ready)(void *, void *, uint8_t force), void (*reap)(void *, void *), void *pd, unsigned tick_usec);

ngx_int_t reaper_flush(reaper_t *rp);
ngx_int_t reaper_stop(reaper_t *rp);
void reaper_each(reaper_t *rp, void (*cb)(void *thing, void *pd), void *pd);


ngx_int_t reaper_add(reaper_t *rp, void *thing);
ngx_int_t reaper_withdraw(reaper_t *rp, void *thing);

#endif /*REAPER_H*/
