#include <ngx_wafflex.h>
#include <util/wfx_redis.h>
static char                   errbuf[512];
static void ngx_wafflex_exit_worker(ngx_cycle_t *cycle);

static ngx_int_t ngx_wafflex_init_preconfig(ngx_conf_t *cf) {  
  ngx_wafflex_init_lua(1);
  return NGX_OK;
}

static ngx_int_t initialize_shm(ngx_shm_zone_t *zone, void *data) {
  if(data) { //zone being passed after restart
    zone->data = data;
    wfx_shm_data = zone->data;
#if nginx_version <= 1011006
    shm_set_allocd_pages_tracker(wfx_shm, &wfx_shm_data->shmem_pages_used);
#endif
  }
  else {
    shm_init(wfx_shm);
    
    if((wfx_shm_data = shm_calloc(wfx_shm, sizeof(*wfx_shm_data), "root shared data")) == NULL) {
      return NGX_ERROR;
    }
#if nginx_version <= 1011006
    wfx_shm_data->shmem_pages_used=0;
    shm_set_allocd_pages_tracker(wfx_shm, &wfx_shm_data->shmem_pages_used);
#endif
    DBG("Shm created with data at %p", wfx_shm_data);
  }
  
  return NGX_OK;
}


static void cachemanager_watchdog_log_fake_writer(ngx_log_t *log, ngx_uint_t level, u_char *buf, size_t len) {
  static int waiting_for_exit = 1;
  if(waiting_for_exit && (ngx_terminate || ngx_quit || ngx_exiting)) {
    waiting_for_exit = 0; //only happens once
    
    //cache manager doesn't call module's exit_worker on exit. do it manually.
    ngx_wafflex_exit_worker((ngx_cycle_t *)ngx_cycle);
  }
}

ngx_log_t cachemanager_watchdog_log;

static int cachemanager_watchdog_added = 0;
static ngx_msec_t wfx_cache_manager(void *data) {
  if(!cachemanager_watchdog_added) {
    ngx_log_t *hacklog = &cachemanager_watchdog_log;
    ngx_memzero(hacklog, sizeof(*hacklog));
    hacklog->next = ngx_cycle->log;
    hacklog->writer = cachemanager_watchdog_log_fake_writer;
    hacklog->log_level = NGX_LOG_DEBUG;
    
    ngx_cycle->log = hacklog;
    
    cachemanager_watchdog_added = 1;
    ERR("hacklog added");
  }
  ERR("managing ze cache");
  return 10000;
}

static ngx_int_t ngx_wafflex_setup_cachemanager_process(ngx_conf_t *cf) {
  ngx_path_t     *path;
  path = ngx_pcalloc(cf->pool, sizeof(ngx_path_t));
  if(path == NULL)
    return NGX_ERROR;
  ngx_str_set(&path->name, "wafflex::ruleset:manager");
  path->manager = (ngx_path_manager_pt )&wfx_cache_manager;
  if(ngx_add_path(cf, &path) != NGX_OK)
    return NGX_ERROR;
  return NGX_OK;
}

static ngx_int_t ngx_wafflex_init_postconfig(ngx_conf_t *cf) {  
  static ngx_str_t   shm_name = ngx_string("wafflex shared data");
  wfx_main_conf_t   *mcf = ngx_http_conf_get_module_main_conf(cf, ngx_wafflex_module);
  if(mcf->shm_size==NGX_CONF_UNSET_SIZE) {
    mcf->shm_size=WAFFLEX_DEFAULT_SHM_SIZE;
  }
  
  if(!mcf->enabled) {
    ERR("wafflex not enabled");
    return NGX_OK;
  }
  wfx_shm = shm_create(&shm_name, &ngx_wafflex_module, cf, mcf->shm_size, initialize_shm, NULL);
  if(!wfx_shm) {
    ERR("unable to create shared memory wrapper");
    return NGX_ERROR;
  }
  if(ngx_wafflex_setup_http_request_hooks(cf) != NGX_OK) {
    ERR("unable to attach http request hooks");
    return NGX_ERROR;
  }
  //ensure the cache manager process runs
  if(ngx_wafflex_setup_cachemanager_process(cf) != NGX_OK) {
    ERR("unable to setup cache manager process");
    return NGX_ERROR;
  }
  
  return NGX_OK;
}

static void *ngx_wafflex_create_main_conf(ngx_conf_t *cf) {
  wfx_main_conf_t      *mcf = ngx_pcalloc(cf->pool, sizeof(*mcf));
  
  mcf->shm_size=NGX_CONF_UNSET_SIZE;
  
  if(mcf == NULL) {
    return NGX_CONF_ERROR;
  }
  return mcf;
}

static void *ngx_wafflex_create_srv_conf(ngx_conf_t *cf) {
  wfx_srv_conf_t      *scf = ngx_pcalloc(cf->pool, sizeof(*scf));
  if(scf == NULL) {
    return NGX_CONF_ERROR;
  }
  return scf;
}
static char *ngx_wafflex_merge_srv_conf(ngx_conf_t *cf, void *prev, void *conf) {
  return NGX_CONF_OK;
}

static void *ngx_wafflex_create_loc_conf(ngx_conf_t *cf) {
  wfx_loc_conf_t      *lcf = ngx_pcalloc(cf->pool, sizeof(*lcf));
  if(lcf == NULL) {
    return NGX_CONF_ERROR;
  }
  
  return lcf;
}

static char *ngx_conf_set_redis_upstream(ngx_conf_t *cf, ngx_str_t *url, void *conf) {  
  ngx_url_t             upstream_url;
  wfx_loc_conf_t       *lcf = conf;
  if (lcf->redis.upstream) {
    return "is duplicate";
  }
  
  ngx_memzero(&upstream_url, sizeof(upstream_url));
  upstream_url.url = *url;
  upstream_url.no_resolve = 1;
  
  if ((lcf->redis.upstream = ngx_http_upstream_add(cf, &upstream_url, 0)) == NULL) {
    return NGX_CONF_ERROR;
  }
  
  lcf->redis.enabled = 1;
  wfx_redis_add_server_conf(cf, lcf);
  
  return NGX_CONF_OK;
}

static char *ngx_wafflex_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child) {
  wfx_loc_conf_t       *prev = parent, *conf = child;
  if(conf->rulesets == NULL) {
    conf->rulesets = prev->rulesets;
  }
  
  ngx_conf_merge_value(conf->redis.url_enabled, prev->redis.url_enabled, 0);
  ngx_conf_merge_value(conf->redis.upstream_inheritable, prev->redis.upstream_inheritable, 0);
  ngx_conf_merge_str_value(conf->redis.url, prev->redis.url, WAFFLEX_REDIS_DEFAULT_URL);
  ngx_conf_merge_str_value(conf->redis.namespace, prev->redis.namespace, "");
  ngx_conf_merge_value(conf->redis.ping_interval, prev->redis.ping_interval, WAFFLEX_REDIS_DEFAULT_PING_INTERVAL_TIME);
  if(conf->redis.url_enabled) {
    conf->redis.enabled = 1;
    wfx_redis_add_server_conf(cf, conf);
  }
  if(conf->redis.upstream_inheritable && !conf->redis.upstream && prev->redis.upstream && prev->redis.upstream_url.len > 0) {
    conf->redis.upstream_url = prev->redis.upstream_url;
    ngx_conf_set_redis_upstream(cf, &conf->redis.upstream_url, conf);
  }
  
  return NGX_CONF_OK;
}

static ngx_int_t ngx_wafflex_init_module(ngx_cycle_t *cycle) {
  //init ipc
  if(wfx_ipc) { //ipc already exists. destroy it!
    ipc_destroy(wfx_ipc);
  }
  wfx_ipc = ipc_init_module("wafflex", cycle);
  //ipc->track_stats = 1;
  ipc_set_alert_handler(wfx_ipc, ngx_wafflex_ipc_alert_handler);
  
  return NGX_OK;
}

static ngx_int_t ngx_wafflex_init_worker(ngx_cycle_t *cycle) {
  ngx_int_t rc = ipc_init_worker(wfx_ipc, cycle);
  if (rc != NGX_OK) {
    ERR("error initializing ipc worker");
    return rc;
  }
  
  if(ngx_process == NGX_PROCESS_WORKER) {
    //reset lua
    ngx_wafflex_shutdown_lua(0);
    ngx_wafflex_init_lua(0);
    ngx_wafflex_init_runtime(0);
  }
  else {
    ngx_wafflex_init_runtime(1);
  }
  return NGX_OK;
}

static void ngx_wafflex_exit_worker(ngx_cycle_t *cycle) {
  ngx_wafflex_shutdown_lua(ngx_process == NGX_PROCESS_WORKER ? 0 : 1);
  ipc_destroy(wfx_ipc);
  shm_destroy(wfx_shm);
}

static void ngx_wafflex_exit_master(ngx_cycle_t *cycle) {
  ngx_wafflex_shutdown_lua(1);
  ipc_destroy(wfx_ipc);
  shm_destroy(wfx_shm);
}

static char *wfx_conf_load_ruleset_file(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
  const char        *errstr;
  wfx_main_conf_t   *mcf = ngx_http_conf_get_module_main_conf(cf, ngx_wafflex_module);
  lua_State         *L = wfx_Lua;
  ngx_str_t         *val = cf->args->elts;
  ngx_int_t          n = cf->args->nelts;
  ngx_str_t         *path, *name;
  if(n==2) { //just the path
    name = NULL;
    path = &val[1];
  }
  else if(n==3) { //name and path
    name = &val[1];
    path = &val[2];
  }
  else {
    return "wrong number of parameters";
  }
  
  lua_getglobal(L, "parseRulesetFile");
  lua_pushlstring(L, (const char *)ngx_cycle->prefix.data, ngx_cycle->prefix.len);
  lua_pushlstring(L, (const char *)path->data, path->len);
  if(name)
    lua_pushlstring(L, (const char *)name->data, name->len); 
  else
    lua_pushnil(L);
  lua_ngxcall(L, 3, 2);
  if (lua_isnil(L, -2)) {
    errstr = lua_tostring(L, -1);
    snprintf(errbuf, 512, "error: %s", errstr);
    lua_pop(L, 2);
    return errbuf;
  }
  else {
    lua_pop(L, 2);
  } 
  mcf->enabled = 1;
  return NGX_CONF_OK;
}

static char *wfx_conf_ruleset(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
  const char           *errstr;
  wfx_loc_conf_t       *lcf = conf;
  wfx_ruleset_conf_t   *rcf;
  ngx_str_t            *name = cf->args->elts;
  ngx_int_t             i, n = cf->args->nelts;

  if(!lcf->rulesets) {
    lcf->rulesets = ngx_array_create(cf->pool, 4, sizeof(*rcf));
  }
  if(!lcf->rulesets) {
    return "couldn't allocate memory for ruleset array";
  }
  
  for(i=1; i<n; i++) {
    if((rcf = ngx_array_push(lcf->rulesets)) == NULL) {
      return "couldn't allocate memory for ruleset data";
    }
    lua_getglobal(wfx_Lua, "deferRulesetCreation");
    lua_pushlstring(wfx_Lua, (const char *)name[i].data, name[i].len);
    lua_pushlightuserdata(wfx_Lua, rcf);
    lua_ngxcall(wfx_Lua, 2, 2);
    if (lua_isnil(wfx_Lua, -2)) {
      if(!lcf->redis.enabled) {
        errstr = lua_tostring(wfx_Lua, -1);
        snprintf(errbuf, 512, "error: %s", errstr);
        lua_pop(wfx_Lua, 2);
        return errbuf;
      }
      else {
        lua_pop(wfx_Lua, 2);
        lua_getglobal(wfx_Lua, "deferRulesetRedisLoad");
        lua_pushlstring(wfx_Lua, (const char *)name[i].data, name[i].len);
        lua_pushlightuserdata(wfx_Lua, lcf);
        lua_pushlightuserdata(wfx_Lua, rcf);
        lua_ngxcall(wfx_Lua, 3, 2);
        if (lua_isnil(wfx_Lua, -2)) {
          errstr = lua_tostring(wfx_Lua, -1);
          snprintf(errbuf, 512, "error: %s", errstr);
          lua_pop(wfx_Lua, 2);
          return errbuf;
        }
      }
    }
    lua_pop(wfx_Lua, 2);
    
    rcf->name = *name;
    rcf->ruleset = NULL;
  }
  
  return NGX_CONF_OK; 
}

static char *ngx_conf_set_redis_url(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
  wfx_loc_conf_t  *lcf = conf;
  
  if(lcf->redis.upstream) {
    return "can't be set here: already using nchan_redis_pass";
  }
  lcf->redis.url_enabled = 1;
  lcf->redis.enabled = 1;
  return ngx_conf_set_str_slot(cf, cmd, conf);
}

//ugly as sin but i don't carrot all
#include <ngx_wafflex_config_commands.c>

static ngx_http_module_t  ngx_wafflex_ctx = {
  ngx_wafflex_init_preconfig,    /* preconfiguration */
  ngx_wafflex_init_postconfig,   /* postconfiguration */
  ngx_wafflex_create_main_conf,  /* create main configuration */
  NULL,                          /* init main configuration */
  ngx_wafflex_create_srv_conf,   /* create server configuration */
  ngx_wafflex_merge_srv_conf,    /* merge server configuration */
  ngx_wafflex_create_loc_conf,   /* create location configuration */
  ngx_wafflex_merge_loc_conf,    /* merge location configuration */
};

ngx_module_t  ngx_wafflex_module = {
  NGX_MODULE_V1,
  &ngx_wafflex_ctx,              /* module context */
  wafflex_commands,              /* module directives */
  NGX_HTTP_MODULE,               /* module type */
  NULL,                          /* init master */
  ngx_wafflex_init_module,       /* init module */
  ngx_wafflex_init_worker,       /* init process */
  NULL,                          /* init thread */
  NULL,                          /* exit thread */
  ngx_wafflex_exit_worker,       /* exit process */
  ngx_wafflex_exit_master,       /* exit master */
  NGX_MODULE_V1_PADDING
};
