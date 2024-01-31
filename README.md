# Wafflex

An Nginx module for rule-based request filtering, rate-limiting, and access control for many Nginx servers sharing rules and data. It's loosely inspired by iptables -- well, not syntactically, or schematically, or even conceptually. I guess iptables is more like a spiritual predecessor, and Wafflex is its own animal designed for doing stuff to HTTP requests rather than network packets.

## Conceptual Overview

*[Rules](#rule)* are simple, non-nesting, non-Turing-complete decisions to [conditionally](#conditions) perform some [actions](#actions) on an HTTP request. Basically, one rule is one [if-then-else](#if-rule) or [switch](#switch-rule) statement. Rules are organized in *[rule lists](#rule-list)*. 

Rules can make use of [*limiters*](#limiters), which are pre-configured limits applied to named counters used for rate-limiting and flag checks.

A *[rule list](#rule-list)* is a sequence of rules that keeps executing rules until the end of the list, unless a rule executes a *[final action](#actions)*. Rule lists are stored in *phase tables*. 

A *[phase table](#phase-table)* specifies lists of rule lists that are run at different phases in the HTTP request-response cycle (after headers, after request completion, after redirect, etc.). Each request phase can have one or more rule lists, which are executed in-order.

Finally, all of the above is stored in a [*rule set*](#rule-set).

## Syntax

### Rule Set
The rule-set is the root-level structure that stores the [phase table](#phase-table), named [rule lists](#rule-list), named [rules](#rule) and [limits](#limits) for [limiters](#limiter).

```js
{ //rule set
  "limits": { 
    "limiterName1": /*limiter*/,
    "limiterName2": /*limiter*/, 
    /*...*/
  }, 
  "phases": /*phase table*/,
  "lists": {   //named lists
    listName1: /*rule list*/, 
    listName2: /*rule list*/, 
    /*...*/
  },
  "rules": {   //named rules
    "ruleName1": /*rule*/, 
    "ruleName2": /*rule*/,
    /*...*/
  }
}
```

A rule set **MUST** have a phase table, but **MAY NOT** have named rules, limiters and lists. The rule lists and named rules **MUST** be uniquely named and can be referenced by name in the phase table. Limiters **MUST** also be uniquely named.

### Phase Table

The phase table keeps track of [rule lists](#rule-list) associated with HTTP request/response phases.
```js
{
  "connect": [ /*rule-list*/, /*rule-list*/, /*...*/ ],
  "headers": [ /*rule-list*/, /*rule-list*/, /*...*/ ],
  "request": [ /*rule-list*/, /*rule-list*/, /*...*/ ]
}
```

The possible HTTP request phases are:

- `connect`:  After a TCP connection is established
- `tls-connect` : After TLS handshake is performed and TLS session is established
- `headers` : After a request's headers are processed
- `body-data` : After part (or all) of a request's body is read
- `request` : After the entire request is processed
- `proxy-response` : After an upstream response is received
- `response-headers` : After response headers are generated, but before they are sent to the client.
- `response-data` : After response headers and body are generated, but before they are sent to the client.
- `response` : After response headers and body are generated, but before they are sent to the client.

Note that initially, a subset of these phases will be implemented (`request`, `response`, and maybe a few more)

Rule lists can be defined inside the phase table, or they can be referenced by name.

### Rule List
An ordered list of rules.
```js
//short-form:
[ /*rule*/, /*rule*/, /*...*/ ]

//long-form
{
  "name": "my-rules", //optional globally unique list name
  "rules": [ /*rule*/, /*rule*/, /*...*/ ]
}
```
Lists declared in the rule set's `lists` hash have their names set to their hash keys:
```js
{ //rule set
  "lists": {
    "myList": [ /*rules*/ ]
  },
  /*...*/
}

//this creates a named list:
{ // rule list
  "name": "myList",
  "rules": [ /*rules*/ ]
}
```

All list names **MUST** be unique to the rule set.

Lists could also be **unordered**, meaning rule execution order is up to the module. Unordered lists can be heavily optimized through adaptive reordering and partial condition evaluation trees. (This is an advanced execution engine feature, and will **not** be part of the MVP.)
```js
{
  "rules": [ /*rule*/, /*rule*/, /*...*/ ],
  "unordered": true
}
```

Rules can be define inside rule lists, and named rules can be referenced by the name:
```js
{ //rule list
  "rules": [ "ruleName1", "ruleName2", {/*rule definition*/} ]
}
```

### Rule

Rules are defined in JSON.
```js
//example rule
{
  "name": "ratelimit",
  "info": "This rule rate-limits by ip",

  "if": {"#limit-break" : {
    "name": "rate-limit",
    "key": "$request_real_ip"
  }},
  "then": [
    {"#tag": "slowdown"},
    {"#reject": {"status": 403, "body": "Slow down!!!"}}
  ]
}
```

Rules have 3 forms:

##### If-rule
```js
{
  "if": /*condition*/
  "then": /*actions*/
  "else": /*actions*/       //optional
}

{
  "if-any": [ /*condition*/, /*condition*/, /*...*/ ],
  "then": /*actions*/
  "else": /*actions*/       //optional
}

{
  "if-all": [ /*condition*/, /*condition*/, /*...*/ ],
  "then": /*actions*/
  "else": /*actions*/       //optional
}
```

`if-any` executes until the first **`true`** condition (think of it as `if(condition1 || condition2 || ...)`)

`if-all` executes until the first **`false`** condition (think of it as `if(condition1 && condition2 && ...)`)

##### Switch-rule
```js
{
  "switch": [
    [ /*condition*/, /*actions*/],
    [ /*condition*/, /*actions*/],
    /*...*/
  ]
}
```
##### Unconditional rule
```js
{
  "do": /*actions*/  
}
```

Rules can also have additional properties, such as `key`, `name`, [`track-stats`](#statistics-and-benchmarking) and [`logging`](#logging) which will be discussed later.

### Actions

Actions are things done to a request, like adding or removing a header, redirecting to an upstream server, rejecting a request with a 400 status code, or closing the connection. An action MAY end the processing of rules for the current request; this is called a **final action**.

```js
/*actions*/       [ /*action*/, /*action*/, /*...*/ ] || /*action*/
/*action*/        "#action-name" || {"#action-name": /*params*/}
/*params*/        { /*...*/ } || [ /*...*/ ] || string || number
```

Arrays of actions are executed in-order until the end of the array, even in the presence of one or more *final actions*. Strings in action parameters are [interpolated](#string-interpolation).

```js
//examples

//array of actions:
[ {"#tag": "processed"}, "#accept" ]
[ "#reject", {"#tag": "rejected"} ] //will be tagged 'rejected' even though '#reject' is a final action.

//single action
"#reject"
{"#reject": 404}
```

### Conditions
Conditions are statements that evaluate to `true` or `false`. They have the same form as an *action*:
```js
/*condition*/        "#condition-name" || {"#condition-name": /*arguments*/}
/*arguments*/        { /*...*/ } || [ /*...*/ ] || string || number
```

As with [actions](#actions), strings in condition arguments are [interpolated](#string-interpolation).
```js
//single condition
"#true" //always true
{"#match": ["$realip_remote_addr", "127.0.0.1"]} // true if request client's ip is 127.0.0.1

```
  
### Limiters
Limiters are used to track a rate, and whether that rate exceeds some threshold. Limiters are defined outside of rules, rule lists, and phase tables.

```js
//limiter syntax
{
  "name": "limiter-name" //globally unique
  "info": "optional limiter description",
  "interval": /*time-interval*/,
  "limit": number //maximum value for that interval
  "sync-steps": integer (default 4) //optional
  "burst": "burst-limiter-name", //optional
  "burst-expire": /*time-interval*/ //optional
}

/*time-interval*/ number (seconds) || nginx-time-range ("10s" /*10 seconds*/ || "1h" /*1 hour*/ || "5d" /*5 days*/ etc)
```

All limiters must have names unique to the [rule set](#rule-set).


## Using Limiters

Limiters are used to perform rate limit checks on numeric *counters* associated with a *key* specified in a rule. This counter is incremented  during the execution of rules, and decreases linearly at a rate of `limit`/`interval` until it reaches 0. The counter values are shared and synchronized between Nginx servers.

A Limiter is checked and the corresponding counter auto-incremented with the [`#limit-break`](#limit-break) condition.

```js
{ //limit
  "name": "request-rate",
  "interval": 60, //seconds
  "limit": 100
}

{ //rule
  "if": {"#limit-break": {
    "name": "request-rate", //limiter name
    "key": "$request_real_ip", //key for current limiter rate value
    //increment: 1 //increment current limit counter value by 1 by default
  }},
  "then": "#reject"
}
```

#### Limiter counter keys

Each limiter has a limit and a *counter* which is stored for that limit at a specified *key*. Different-named limiters store their own counters at the same *key*:

```js
{ //limit
  "name": "request-rate",
  "interval": 60, //seconds
  "limit": 100
}

{ //another limit
  "name": "weekly-allowance",
  "interval": '7d', //7 days
  "limit": 1000000 // a million requests a week
}

{ //rule
  "if-any": [
    {"#limit-break": {
      name: "request-rate", //limiter name
      key: "$request_real_ip", //key for current limiter rate value
    }},
    {"#limit-break": {
      name: "weekly-allowance",
      key: "$request_real_ip" // same key as above, but tracking a different limiter's counter
    }}
  ],
  "then": "#reject"
}
```
 
 Because it is common to use the same key for multiple limiter counters, the default *key* can be specified at the rule level:
 
```js
{ //limit
  "name": "request-rate",
  "interval": 60, //seconds
  "limit": 100
}

{ //another limit
  "name": "weekly-allowance",
  "interval": '7d', //7 days
  "limit": 1000000 // a million requests a week
}

{ //rule
  "key": "$request_real_ip", //default key for limiter counters

  "if-any": [
    {"#limit-break": "request-rate"}, //uses default rule-level key
    {"#limit-break": "weekly-allowance"} //same as above
  ],
  "then": "#reject"
}
```
 
By default, the [`#limit-break`](#limit-break) [condition](#condition) auto-increments the numeric value stored at key specified by `key`. To test a limiter without incrementing the counter, use [`#limit-check`](#limit-check), or explicitly set the `increment` argument for `#limit-break` to 0. A limiter counter can also be incremented as an [action][#actions] with [`#limit-increment`](#limit-increment)

#### Bursty limits
Two limiters can be combined to create a *limiter with a burst rate*:
```js 
{ //limit
  "name": "burstable-rate-limit",
  "interval": 60, //seconds
  "limit": 100,
  "burst": "burst-rate"
  "burst-expire": "1h"
}

{ //burst rate
  "name": "burst-limit",
  "interval": "1h",
  "limit": 100000 // burst at 100K/hour
}

{ // rule using the bursty limit
  if: {'#limit-break': "burstable-rate-limit"},
  then: "#reject"
}
```

Here, the `burstable-rate-limit` has a burst rate defined by `burst-limit`. This means `burstable-rate-limit` starts rate-limiting only after the `burst-limit` has been exceeded. In the above case, the `burstable-rate-limit`(100/hour) is active only after `burst-limit` (100K/hour) is reached. The `burstable-rate-limit` remains in effect as long as `burst-limit` is exceeded, or for `burst-expire` amount of time after the `burst-limit` is first exceeded -- whichever is longer.



#### Limiter counter sharing

Limiter counters can be shared between Nginx instances. This allows for distributed rate tracking among a collection of servers. By default, counters are shared every time the value increases by 25% of the `limit`. 

##### sync-steps
The `sync-steps` value controls how often counters are shared between Nginx instances. Between 0 and `limit`, the counter value will be shared `sync-steps` times. Put another way, the counter is shared every time its value increases by `limit`/`sync-steps`. Some examples:

- `sync-steps: 0`: counter value is never shared with other Nginx instances.
- `sync-steps: 1`: counter value is shared when the limit is reached, doubled, tripled, etc.
- `sync-steps: 4`: counter value is shared every time it is increased by `limit`/4, i.e. every 25% of the `limit`
- `sync-steps: 100`: counter value is shared every time it increases by 1% or `limit`, or 100 times between 0 and `limit`, between `limit` and 2 * `limit` and so on.


#### Limiters as flags
Limiters with a `limit` of 1 can behave like shared boolean flags with an expiration time.

```js
{//limit
  "name": "ip-ban",
  "limit": 1,
  "interval": "1d"
  //ban flag that stays active for 1 day
}

{ //ban the ip when the request has a Ban-Me: 1 header
  "if": {"#match": ["$http_ban_me", "1"]},
  "then": {"#flag": {"name": "ip-ban", "key": "$request_real_ip"}} //flag this guy as banned
}

{ //check if banned
  "if": {"#flag-check": {name: "ip-ban", key: "$request_real_ip"}},
  "then": "#reject" // you're banned, dude. go home.
}
```

`#flag-check` is the same as `#limit-check`, and `#flag` same as `#limit-increment`.

## String Interpolation

  Strings passed to [conditions](#condition) and [actions](#action) are automatically interpolated using the Nginx string interpolation syntax, with Nginx request variables.
```js
"static string"
"$http_host" //evaluates to the value of the Host header
"foo/bar/${request_real_ip}:$url" //compound interpolated string
```

  There's some room for heavy optimization in reusing interpolated strings, especially when used as keys, or if they are going to be hashed. Optimizations may not be present in MVP.

## Condition List

#### `#match`
match strings for equality
```js
{"#match": [ "string1", "string2", /*...*/ ]} //must all be the same to be true
```
   
#### `#match-regex`
match a string by regular expression
```js
{"#match-regex": [ "string", "/regular_expression.*/"]}
```
Note that if the regular expression has any [interpolation](#string-interpolation) variables in it, it cannot be optimized and will need to be recompiled on every request.

  <img align="right" src="https://i.imgur.com/RuPsJV5.gif" />
  
#### `#limit-break`
Increment limiter counter, and check if a limit has been broken (exceeded)
```js
{"#limit-break": {
  "name": 'limit-name',
  "key": "limiter-counter-key",
  "increment": 1 //default value
}}
```
If `key` is absent, the [rule](#rule)'s default `key` is used.
    
#### `#limit-check`
Same as [`#limit-break`](#limit-break), but with `increment: 0`
   
#### `#flag-check`
Same as [`#limit-break`](#limit-break). Useful for Limiters with `limit: 1` that are being used as flags.

#### `#tag-check`
Checks if a tag has been set for the request by the [`#tag`](#tag) command.
```js
{"#tag-check": "tag-name"}
```


#### `#match-subrequest`
Perform a non-blocking subrequest to an Nginx location, and compare the response to a set of matching conditions.  
*(NOT included for MVP)*
```js
{"#match-subrequest": [/*subrequest*/, /*match_conditions*/]}

/*subrequest*/
"/subrequest_path" //HTTP GET subrequest to this path

{ 
  "path": "/subrequest_path",
  "method": "GET" // default
  "forward-headers": true //default. use headers from incoming request,
  "set-headers": {"Header-Name": "$http_header_name", "Other-Header-Name": "foobar"}
}

/*match_conditions*/
"ok" // response code 200-299
number // match response code number
{
  "code": number || [ code1, code2, code3 ],
  "headers": {
    "Header-Name": "match-Header-Value"
  },
  "body": "match-body-value"
}
```

#### `#true`
Always true. Useful as a default switch condition.

#### `#false`
Always false. Might be useful when writing rules.

## Action List

#### `#reject`
Reject a request. This is a [**final action**](#actions), after which no more [*rules*](#rule) will be processed for this request.
```js 
//short-form
"#reject"

//long-form
{"#reject": {
  "status": number, //HTTP response status code, default 403
  "body": "response_body" //HTTP response body, optinal
}}
```

#### `#accept`
Accept a request. This is a [**final action**](#actions), after which no more [*rules*](#rule) will be processed for this request.
```js
//short form
"#accept"
```
#### `#proxy-pass`
Pass request to an upstream proxy. See the Nginx [`proxy_pass`](https://nginx.org/en/docs/http/ngx_http_proxy_module.html#proxy_pass) command.  
This is a [**final action**](#actions), after which no more [*rules*](#rule) will be processed for this request.
```js
{'#proxy-pass': "http://hostname-or-upstream-name/path"}
```


#### `#proxy-set-header`
Set proxy header. See the Nginx [`proxy_set_header`](https://nginx.org/en/docs/http/ngx_http_proxy_module.html#proxy_set_header) command.
```js
//set one or many headers
{'#proxy-set-header': {
  "Proxy-Header-Name":"$http_header_name",
  "X-Real-IP": "$request_real_ip"
}}
```

#### `#limit-increment`
Increment limit counter
```js
//short form (if rule-wide default key is provided)
{"#limit-increment": "limiter-name"}

//long-form
{"#limit-increment": {
  "name": "limiter-name",
  "key": "limit-counter-key", //can be omitted if rule-level key given
  "increment": 1 //default
}}
```
  
#### `#flag`
Increment flag (limiter with `limit:1`) by 1. Same as `#limit-increment`
```js
//short form (if rule-wide default key is provided)
{"#flag": "flag-name"}

//long-form
{"#flag": {
  "name": "flag-name",
  "key": "flag-key", //can be omitted if rule-level key given
  "increment": 1 //default
}}
```
    
#### `#limit-reset`
Set limit counter back to 0.
```js
//short form (if rule-wide default key is provided)
{"#limit-reset": "limit-name"}

//long-form
{"#limit-reset": {
  "name": 'limit-name',
  "key": "limit-counter-key" //can be omitted if rule-level key given
}}
```
   
#### `#flag-reset`
Set flag back to 0. Same as [`#limit-reset`](#limit-reset)

#### `#tag`
Set a string tag for the request. which adds a `RoF-Tag-<tagname>: 1` header to the request.
```js
{"#tag": "tag-name"}
```

#### `#tag-reset`
Deletes the given string tag for the request. which also removes the `RoF-Tag-<tagname>: 1` header from the request. If tag is already absent, does nothing.
```js
{"#tag-reset": "tag-name"}
```

#### `#subrequest`
Perform a non-blocking subrequest to an Nginx location.  
*(NOT included for MVP)*
```js
{"#subrequest": "/subrequest_path"}

{"#subrequest": {
    "path": "/subrequest_path",
    "forward-headers": true //default. use headers from incoming request,
    "set-headers": {"Header-Name": "$http_header_name", "Other-Header-Name": "foobar"}
  }
}
```
   
## Synchronization
Rule sets can be loaded from Redis, a file, or Consul on Nginx startup. (Consul may be omitted for MVP).
During runtime, rule sets, lists, limits, and counters are stored on a Redis server or cluster. (Cluster support may be omitted for MVP). 
Rule sets, lists, limits, and rules can be created, updated, and deleted at runtime via a command-line tool which runs updating commands on the Redis server. In turn, Redis notifies all the Nginx servers, and data is updated as-needed.

## Statistics and Benchmarking
Aggregate rule execution information will be available for logging through variables available upon request completion:

- `$rof_request_time` : time to run all rules
- `$rof_request_rules` : number of rules processed
- `$rof_request_rules_percent` : percent of rules processed
- `$rof_request_final_rule` : name/id of rule that either accepted or rejected the request (empty if neither)
- `$rof_request_final_list` : name/id of list that contained the final rule (empty if none)
- `$rof_request_final_phase` : name of phase in the [phase table](#phase-table)

Individual rule statistics are _not_ collected by default, but can be enabled on specific rules with the `track-stats` property:
```js
{
  "if": /*condition*/,
  "then": /*action*/
  "track-stats": true
}
```

With `track-stats` enabled, runtime statistics for the rule will be collected, including totals for  number of times executed, and the number of timers a request was rejeced via the rule. This data can be accessed via an Nginx location akin to the [`stub_status`](http://nginx.org/en/docs/http/ngx_http_stub_status_module.html#stub_status) setting.

## Logging

Specific rules' processing sequence can be enabled with the `log` property:
```js
{
  "if": /*condition*/,
  "then": /*action*/
  "log": true
}
```

This will log each step of the rule's execution to a separate Wafflex log using the standard Nginx error log format, with the text being presented in a human-readable format.

Additionally, the logging of requests matching some pattern may be enabled in a manner to be determined later. (Not part of MVC)

