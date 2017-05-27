#include <ngx_wafflex.h>

  static char                   errbuf[512];

static ngx_int_t ngx_wafflex_init_preconfig(ngx_conf_t *cf) {  
  ngx_wafflex_init_lua();
  return NGX_OK;
}

static ngx_int_t initialize_shm(ngx_shm_zone_t *zone, void *data) {
  wfx_shm_data_t     *d;
  if(data) { //zone being passed after restart
    zone->data = data;
    d = zone->data;
#if nginx_version <= 1011006
    shm_set_allocd_pages_tracker(wfx_shm, &d->shmem_pages_used);
#endif
  }
  else {
    shm_init(wfx_shm);
    
    if((d = shm_calloc(wfx_shm, sizeof(*d), "root shared data")) == NULL) {
      return NGX_ERROR;
    }
#if nginx_version <= 1011006
    d->shmem_pages_used=0;
    shm_set_allocd_pages_tracker(wfx_shm, &d->shmem_pages_used);
#endif
    DBG("Shm created with data at %p", d);
  }
  
  
  //we can create all the rulesets now
  lua_getglobal(wfx_Lua, "createDeferredRulesets");
  lua_ngxcall(wfx_Lua, 0, 0);
  
  return NGX_OK;
}

static ngx_int_t ngx_wafflex_init_postconfig(ngx_conf_t *cf) {  
  static ngx_str_t   shm_name = ngx_string("wafflex shared data");
  wfx_main_conf_t   *mcf = ngx_http_conf_get_module_main_conf(cf, ngx_wafflex_module);
  if(mcf->shm_size==NGX_CONF_UNSET_SIZE) {
    mcf->shm_size=WAFFLEX_DEFAULT_SHM_SIZE;
  }
  wfx_shm = shm_create(&shm_name, &ngx_wafflex_module, cf, mcf->shm_size, initialize_shm, NULL);
  
  ngx_wafflex_setup_http_request_hooks(cf);
  
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
static char *ngx_wafflex_merge_loc_conf(ngx_conf_t *cf, void *prev, void *conf) {
  wfx_loc_conf_t      *lcf = conf;
  wfx_loc_conf_t      *prev_lcf = prev;
  if(lcf->rulesets == NULL) {
    lcf->rulesets = prev_lcf->rulesets;
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
  return ipc_init_worker(wfx_ipc, cycle);
}


static void ngx_wafflex_exit_worker(ngx_cycle_t *cycle) {
  ngx_wafflex_shutdown_lua();
  ipc_destroy(wfx_ipc);
}

static void ngx_wafflex_exit_master(ngx_cycle_t *cycle) {
  ngx_wafflex_shutdown_lua();
  ipc_destroy(wfx_ipc);
}


//converts string to positive double float
static double wfx_atof(u_char *line, ssize_t n) {
  ssize_t cutoff, cutlim;
  double  value = 0;
  
  u_char *decimal, *cur, *last = line + n;
  
  if (n == 0) {
    return NGX_ERROR;
  }

  cutoff = NGX_MAX_SIZE_T_VALUE / 10;
  cutlim = NGX_MAX_SIZE_T_VALUE % 10;
  
  decimal = memchr(line, '.', n);
  
  if(decimal == NULL) {
    decimal = line + n;
  }
  
  for (n = decimal - line; n-- > 0; line++) {
    if (*line < '0' || *line > '9') {
      return NGX_ERROR;
    }

    if (value >= cutoff && (value > cutoff || (*line - '0') > cutlim)) {
      return NGX_ERROR;
    }

    value = value * 10 + (*line - '0');
  }
  
  double decval = 0;
  
  for(cur = (decimal - last) > 10 ? decimal + 10 : last-1; cur > decimal && cur < last; cur--) {
    if (*cur < '0' || *cur > '9') {
      return NGX_ERROR;
    }
    decval = decval / 10 + (*cur - '0');
  }
  value = value + decval/10;
  
  return value;
}

static ssize_t wfx_parse_size(ngx_str_t *line) {
  u_char   unit;
  size_t   len;
  ssize_t  size, scale, max;
  double   floaty;
  
  len = line->len;
  unit = line->data[len - 1];

  switch (unit) {
  case 'K':
  case 'k':
      len--;
      max = NGX_MAX_SIZE_T_VALUE / 1024;
      scale = 1024;
      break;

  case 'M':
  case 'm':
      len--;
      max = NGX_MAX_SIZE_T_VALUE / (1024 * 1024);
      scale = 1024 * 1024;
      break;
  
  case 'G':
  case 'g':
      len--;
      max = NGX_MAX_SIZE_T_VALUE / (1024 * 1024 * 1024);
      scale = 1024 * 1024 * 1024;
      break;

  default:
      max = NGX_MAX_SIZE_T_VALUE;
      scale = 1;
  }

  floaty = wfx_atof(line->data, len);
  
  if (floaty == NGX_ERROR || floaty > max) {
      return NGX_ERROR;
  }

  size = floaty * scale;

  return size;
}

//config helpers
static char *wfx_conf_set_size_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
  char  *p = conf;

  size_t           *sp;
  ngx_str_t        *value;
  ngx_conf_post_t  *post;


  sp = (size_t *) (p + cmd->offset);
  if (*sp != NGX_CONF_UNSET_SIZE) {
      return "is duplicate";
  }

  value = cf->args->elts;

  *sp = wfx_parse_size(&value[1]);
  if (*sp == (size_t) NGX_ERROR) {
    return "invalid value";
  }

  if (cmd->post) {
    post = cmd->post;
    return post->post_handler(cf, post, sp);
  }

  return NGX_CONF_OK;
}


static char *wfx_conf_load_ruleset(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
  const char                   *errstr;
  lua_State                    *L = wfx_Lua;
  ngx_str_t                    *val = cf->args->elts;
  ngx_int_t                     n = cf->args->nelts;
  ngx_str_t                    *path, *name;
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
      errstr = lua_tostring(wfx_Lua, -1);
      snprintf(errbuf, 512, "error: %s", errstr);
      lua_pop(wfx_Lua, 2);
      return errbuf;
    }
    lua_pop(wfx_Lua, 2);
    
    rcf->name = *name;
    rcf->ruleset = NULL;
  }
  
  return NGX_CONF_OK; 
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
