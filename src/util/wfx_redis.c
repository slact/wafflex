#include "wfx_redis.h"
#include <ngx_wafflex.h>
#include <util/wfx_lua.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <ngx_wafflex_hiredis_adapter.h>
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
  
  lua_rawgeti(wfx_Lua, LUA_REGISTRYINDEX, ref);
  assert(lua_type(wfx_Lua, -1) == LUA_TFUNCTION);
  
  if(reply == NULL) { //redis disconnected?...
    lua_pushnil(wfx_Lua);
    if(c->err) {
      lua_pushstring(wfx_Lua, c->errstr);
    }
    else {
      lua_pushliteral(wfx_Lua, "got a NULL redis reply for unknown reason");
    }
  }
  else if(reply->type == REDIS_REPLY_ERROR) {
    lua_pushnil(wfx_Lua);
    lua_pushstring(wfx_Lua, reply->str);
  }
  else {
    //all ok
    push_reply(wfx_Lua, reply);
    lua_pushnil(wfx_Lua);
  }
  
  lua_ngxcall(wfx_Lua, 2, 0);
  luaL_unref(wfx_Lua, LUA_REGISTRYINDEX, ref);
}

int redis_command(lua_State *L) {
  const char        *argv[LUAHIREDIS_MAXARGS];
  size_t             argvlen[LUAHIREDIS_MAXARGS];
  int                nargs;
  int                ref;
  
  ERR("redis_command");
  
  redisAsyncContext *ctx = lua_touserdata(L, 1);
  assert(lua_isfunction(L, 2));
  
  nargs = load_args(L, 3, argv, argvlen);
  
  lua_pushvalue(L, 2);
  ref = luaL_ref(L, LUA_REGISTRYINDEX);
  
  redisAsyncCommandArgv(ctx, resume_lua, (void *)(intptr_t )ref, nargs, argv, argvlen);
  
  lua_pushboolean(L, 1);
  return 1;
}
int redis_loadscripts(lua_State *L) {
  return 0;
}
int wfx_lua_timeout(lua_State *L) {
  return 0;
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
  redis_nginx_init();
  wfx_lua_loadscript(wfx_Lua, redis);
  wfx_lua_register(wfx_Lua, redis_connect);
  wfx_lua_register(wfx_Lua, redis_close);
  wfx_lua_register(wfx_Lua, redis_command);
  wfx_lua_register(wfx_Lua, redis_loadscripts);
  wfx_lua_register(wfx_Lua, wfx_lua_timeout);
  wfx_lua_register(wfx_Lua, wfx_lua_hiredis_get_peername);
  lua_ngxcall(wfx_Lua, 6, 0);
  return NGX_OK;
}

int wfx_redis_init_runtime(lua_State *L, int manager) {
  if (!manager) {
    return 0;
  }
  lua_getglobal(L, "connectRedises");
  lua_ngxcall(L, 0, 0);
  return 1;
}
