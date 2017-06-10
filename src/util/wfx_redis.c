#include "wfx_redis.h"
#include <ngx_wafflex.h>
#include <util/wfx_lua.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <ngx_wafflex_hiredis_adapter.h>
#include <ngx_wafflex_redis_lua_scripts.h>
#include <assert.h>

#define LUAHIREDIS_MAXARGS (256)

int wfx_redis_add_server_conf(ngx_conf_t *cf, wfx_loc_conf_t *lcf) {
  ngx_str_t  str;
  char      *buf;
  wfx_lua_getfunction(wfx_Lua, "registerRedis");
  lua_pushngxstr(wfx_Lua, &lcf->redis.url);
  lua_ngxcall(wfx_Lua, 1, 1);
  
  lua_tongxstr(wfx_Lua, -1, &str);
  buf = ngx_palloc(cf->pool, str.len);
  ngx_memcpy(buf, str.data, str.len);
  lcf->redis.url.len = str.len;
  lcf->redis.url.data = (u_char *)buf;
  lua_pop(wfx_Lua, 1);
  return 1;
}


int redis_connect(lua_State *L) {
  redisAsyncContext *ctx;
  const char *hostname = lua_tostring(L, 1);
  int         port = lua_tonumber(L, 2);
  ctx = redisAsyncConnect(hostname, port);
  if (ctx->err) {
    lua_pushnil(L);
    lua_pushstring(L, ctx->errstr);
    return 2;
  }
  
  if(redis_nginx_event_attach(ctx) != REDIS_OK) {
    lua_pushnil(L);
    lua_pushliteral(L, "could not attach nginx events to redis async context");
    return 2;
  }
  
  lua_pushlightuserdata(L, ctx);
  return 1;
}

int redis_close(lua_State *L) {
  redisAsyncContext *ctx = lua_touserdata(L, 1);
  if(!ctx) {
    luaL_error(L, "expected a redisAsyncContext for redis_close, got NULL");
  }
  redisAsyncFree(ctx);
  return 0;
}

static int load_args(lua_State *L, int first_index, const char ** argv, size_t * argvlen) {
  int nargs = lua_gettop(L) - first_index + 1;
  int i = 0;
  
  if (nargs <= 0)
    return luaL_error(L, "missing command name");
  if (nargs > LUAHIREDIS_MAXARGS)
    return luaL_error(L, "too many arguments");
  
  for (i = 0; i < nargs; ++i) {
    size_t len = 0;
    const char *str = lua_tolstring(L, first_index + i, &len);
    if (str == NULL)
      return luaL_argerror(L, first_index + i, "expected a string or number value");
    argv[i] = str;
    argvlen[i] = len;
  }
  
  return nargs;
}

static int push_reply(lua_State *L, redisReply *pReply) {
  switch (pReply->type) {
    case REDIS_REPLY_STATUS:
      luaL_checkstack(L, 1, "not enough stack to push reply");
      lua_pushlstring(L, pReply->str, pReply->len); /* name */
      break;

    case REDIS_REPLY_ERROR:
      /* Not caching errors, they are (hopefully) not that common */
      raise(SIGABRT); //don't know how to do this
      break;

    case REDIS_REPLY_INTEGER:
      luaL_checkstack(L, 1, "not enough stack to push reply");
      lua_pushinteger(L, pReply->integer);
      break;

    case REDIS_REPLY_NIL:
      luaL_checkstack(L, 1, "not enough stack to push reply");
      lua_pushboolean(L, 0);
      break;

    case REDIS_REPLY_STRING:
      luaL_checkstack(L, 1, "not enough stack to push reply");
      lua_pushlstring(L, pReply->str, pReply->len);
      break;

    case REDIS_REPLY_ARRAY:
    {
      unsigned int i = 0;

      luaL_checkstack(L, 2, "not enough stack to push reply");

      lua_createtable(L, pReply->elements, 0);

      for (i = 0; i < pReply->elements; ++i)
      {
        /*
        * Not controlling recursion depth:
        * if we parsed the reply somehow,
        * we hope to be able to push it.
        */

        push_reply(L, pReply->element[i]);
        lua_rawseti(L, -2, i + 1); /* Store sub-reply */
      }

      break;
    }

    default: /* should not happen */
      return luaL_error(L, "command: unknown reply type: %d", pReply->type);
  }

  /*
  * Always returning a single value.
  * If changed, change REDIS_REPLY_ARRAY above.
  */
  return 1;
}

static void resume_lua(redisAsyncContext *c, void *rep, void *privdata) {
  intptr_t    ref = (intptr_t )(char *)privdata;
  redisReply *reply = rep;
  int         coroutine;
  lua_State  *L;
  
  wfx_lua_pushfromref(wfx_Lua, ref);
  if(lua_isfunction(wfx_Lua, -1)) {
    coroutine = 0;
    L = wfx_Lua;
  }
  else {
    assert(lua_isthread(wfx_Lua, -1));
    L = lua_tothread(wfx_Lua, -1);
    lua_pop(L, 1);
    coroutine = 1;
  }
  
  if(reply == NULL) { //redis disconnected?...
    lua_pushnil(L);
    if(c->err) {
      lua_pushstring(L, c->errstr);
    }
    else {
      lua_pushliteral(L, "got a NULL redis reply for unknown reason");
    }
  }
  else if(reply->type == REDIS_REPLY_ERROR) {
    lua_pushnil(L);
    lua_pushstring(L, reply->str);
  }
  else {
    //all ok
    push_reply(L, reply);
    lua_pushnil(L);
  }
  
  //lua_printstack(wfx_Lua);
  
  if(coroutine) {
    //ERR("COROSTACK");
    //lua_printstack(L);
    wfx_lua_resume(L, 2);
    wfx_lua_unref(wfx_Lua, ref);
    lua_pop(wfx_Lua, 1);
  }
  else {
    lua_ngxcall(L, 2, 1);
    if(lua_isfunction(L, -1)) {
      lua_rawseti(L, LUA_REGISTRYINDEX, ref);
    }
    else {
      wfx_lua_unref(wfx_Lua, ref);
      lua_pop(wfx_Lua, 1);
    }
  }
  //lua_printstack(wfx_Lua);
}

int redis_command(lua_State *L) {
  const char        *argv[LUAHIREDIS_MAXARGS];
  size_t             argvlen[LUAHIREDIS_MAXARGS];
  int                nargs;
  int                ref;
  int                yield;
  
  redisAsyncContext *ctx = lua_touserdata(L, 1);
  if(lua_isfunction(L, 2)) {
    yield = 0;
  }
  else {
    assert(lua_isthread(L, 2));
    yield = 1;
  }
  
  nargs = load_args(L, 3, argv, argvlen);
  
  ref = wfx_lua_ref(L, 2);
  
  redisAsyncCommandArgv(ctx, resume_lua, (void *)(intptr_t )ref, nargs, argv, argvlen);
  
  if(!yield) {
    lua_pushboolean(L, 1);
    return 1;
  }
  return lua_yield(L, 0);
}

int redis_subscribe(lua_State *L) {
  const char        *argv[LUAHIREDIS_MAXARGS];
  size_t             argvlen[LUAHIREDIS_MAXARGS];
  int                nargs;
  int                ref;
  int                yield;
  
  redisAsyncContext *ctx = lua_touserdata(L, 1);
  if(lua_isfunction(L, 2)) {
    yield = 0;
  }
  else {
    assert(lua_isthread(L, 2));
    yield = 1;
  }
  
  nargs = load_args(L, 3, argv, argvlen);
  
  ref = wfx_lua_ref(L, 2);
  
  redisAsyncCommandArgv(ctx, resume_lua, (void *)(intptr_t )ref, nargs, argv, argvlen);
  
  if(!yield) {
    lua_pushboolean(L, 1);
    return 1;
  }
  return lua_yield(L, 0);
}


static int wfx_lua_hiredis_get_peername(lua_State *L) {
  
  redisAsyncContext     *ctx = lua_touserdata(L, 1);
  char                  ipstr[INET6_ADDRSTRLEN+1];
  struct sockaddr_in    *s4;
  struct sockaddr_in6   *s6;
  
  // deal with both IPv4 and IPv6:
  switch(ctx->c.sockaddr.sa_family) {
    case AF_INET:
      s4 = (struct sockaddr_in *)&ctx->c.sockaddr;
      inet_ntop(AF_INET, &s4->sin_addr, ipstr, INET6_ADDRSTRLEN);
      break;
    case AF_INET6:
      s6 = (struct sockaddr_in6 *)&ctx->c.sockaddr;
      inet_ntop(AF_INET6, &s6->sin6_addr, ipstr, INET6_ADDRSTRLEN);
      break;
    case AF_UNSPEC:
      DBG("sockaddr info not available");
      return NGX_ERROR;
    default:
      DBG("unexpected sockaddr af family");
      return NGX_ERROR;
  }
  
  lua_pushstring(L, ipstr);
  return 1;
}

ngx_int_t ngx_wafflex_init_redis(void) {
  wfx_redis_lua_script_t  *cur;
  int                      n = 1;
  redis_nginx_init();
  wfx_lua_loadscript(wfx_Lua, redis);
  wfx_lua_register(wfx_Lua, redis_connect);
  wfx_lua_register(wfx_Lua, redis_close);
  wfx_lua_register(wfx_Lua, redis_command);
  wfx_lua_register(wfx_Lua, wfx_lua_timeout);
  wfx_lua_register(wfx_Lua, wfx_lua_hiredis_get_peername);
  
  lua_createtable(wfx_Lua, wfx_redis_lua_scripts_count, 0);
  WFX_REDIS_LUA_SCRIPTS_EACH(cur) {
    lua_createtable(wfx_Lua, 0, 3);
    
    lua_pushstring(wfx_Lua, cur->name);
    lua_setfield(wfx_Lua, -2, "name");
    
    lua_pushstring(wfx_Lua, cur->hash);
    lua_setfield(wfx_Lua, -2, "hash");
    
    lua_pushstring(wfx_Lua, cur->script);
    lua_setfield(wfx_Lua, -2, "src");
    
    lua_rawseti(wfx_Lua, -2, n++);
  }
  lua_ngxcall(wfx_Lua, 6, 0);
  
  return NGX_OK;
}

int wfx_redis_init_runtime(lua_State *L, int manager) {
  if (!manager) {
    //ngx_wafflex_init_redis();
    //lua_getglobal(L, "testRedisConnector");
    //lua_ngxcall(L, 0, 0);
    return 0;
  }
  
  //lua_getglobal(L, "testRedisConnector");
  //lua_ngxcall(L, 0, 0);
  
  lua_getglobal(L, "connectRedises");
  lua_ngxcall(L, 0, 0);
  return 1;
}
