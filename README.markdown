Name
====

ngx-dynamic-healthcheck - Nginx upstreams healthchecks.

Module requires [zone](https://nginx.org/en/docs/http/ngx_http_upstream_module.html#zone) upstream directive.

* support http, tcp, ssl checks.
* support dynamic reconfiguration
* support persistance of healthcheck parameters
* optionally support LUA API for reconfiguration
* with [dynamic-upstream-module](https://github.com/ZigzagAK/ngx_dynamic_upstream) automaticaly cover by healthchecks new added peers (with DNS balancing).
* TCP checks with requrest/response (for Redis for example)
* pattern matching
* disabling hosts

This module supports http and stream upstream types.

Build status
======
[![Build Status](https://travis-ci.org/ZigzagAK/ngx_dynamic_healthcheck.svg?branch=master)](https://travis-ci.org/ZigzagAK/ngx_dynamic_healthcheck)

Table of Contents
=================

* [Name](#name)
* [Status](#status)
* [Description](#description)
* [Install](#install)
* [Synopsis](#synopsis)
* [Configuration directives](#configuration-directives)
    * [Individual upstream parameters](#individual_upstream_parameters)
        - [check](#check)
        - [check_request_uri](#check_request_uri)
        - [check_request_headers](#check_request_headers)
        - [check_request_body](#check_request_body)
        - [check_response_codes](#check_response_codes)
        - [check_response_body](#check_response_body)
        - [check_persistent](#check_persistent)
        - [check_disable_host](#check_disable_host)
        - [check_exclude_host](#check_exclude_host)
    * [Global healthcheck parameters](#global_healthcheck_parameters)
        - [healthcheck](#healthcheck)
        - [healthcheck_request_uri](#healthcheck_request_uri)
        - [healthcheck_request_headers](#healthcheck_request_headers)
        - [healthcheck_request_body](#healthcheck_request_body)
        - [healthcheck_response_codes](#chealthheck_response_codes)
        - [healthcheck_response_body](#chealthheck_response_body)
        - [healthcheck_persistent](#healthcheck_persistent)
        - [healthcheck_disable_host](#healthcheck_disable_host)
    * [Reconfiguration API](#reconfiguration_api)
        - [get](#healthcheck_get)
        - [status](#healthcheck_status)
        - [update](#healthcheck_update)
* [LUA API](#lua)
    * [get](#lua_get)
    * [update](#lua_update)
    * [disable](#lua_disable)
    * [disable_host](#lua_disable_host)
    * [status](#lua_status)

Status
====

This module is under development.

Description
=======

This module provides dynamicaly configurable healthchecks.

[Back to TOC](#table-of-contents)

Install
====

Build nginx.
All dependencies are downloaded automaticaly.

```
git clone git@github.com:ZigzagAK/ngx_dynamic_healthchecks.git
cd ngx_dynamic_healthchecks
./build.sh
```

Archive will be placed in the `install` folder after successful build.

[Back to TOC](#table-of-contents)

Synopsis
=====

```
http {
  healthcheck fall=2 rise=2 interval=60 timeout=10000 type=tcp;
  healthcheck_persistent healthcheck;

  healthcheck_disable_host balancer01.domain.net:9000;
  healthcheck_disable_host balancer02.domain.net:9000;

  server {
    listen 8888;
    location = /healthcheck/get {
        healthcheck_get;
    }
    location = /healthcheck/status {
        healthcheck_status;
    }
    location = /healthcheck/update {
        healthcheck_update;
    }
  }

  upstream mail {
      zone mail 128k;

      # https://github.com/ZigzagAK/ngx_dynamic_upstream ver >= 2.X.X
      dns_update 10s;
      dns_ipv6 off;

      server mail.ru:443 down;

      check fall=2 rise=2 interval=10 timeout=10000 type=ssl;
  }

  upstream google {
      zone google 128k;

      # https://github.com/ZigzagAK/ngx_dynamic_upstream ver >= 2.X.X
      dns_update 10s;
      dns_ipv6 off;

      server mail.ru down;

      check fall=2 rise=2 interval=10 timeout=10000 type=tcp;
  }

  upstream test {
      zone test 128k;

      # https://github.com/ZigzagAK/ngx_dynamic_upstream ver >= 2.X.X
      dns_update 10s;
      dns_ipv6 off;

      server balancer.domain.net:9000 down;

      check fall=2 rise=2 interval=10 timeout=10000 type=http;
      check_request_uri GET /health;
      check_request_body ping;
      check_response_body .*;
      check_request_headers a=1 b=2;
      check_response_codes 200 204;
  }
}

stream {
    upstream redis-ro {
      zone shm_redis-ro 128k;

      server 127.0.0.1:6379 down;
      server 127.0.0.2:6379 down;
      server 127.0.1.1:6379 down;
      server 127.0.1.2:6379 down;

      check fall=1 rise=2 timeout=10000 interval=10;

      check_request_body "multi\r\ninfo replication\r\nget x\r\nexec\r\nquit\r\n";
      check_response_body "(\+OK).*(\+QUEUED).*(\+QUEUED).*(role:slave).*(master_link_status:up).*(master_sync_in_progress:0).*(check).*(\+OK)";
    }

    upstream redis-rw {
      zone shm_redis-rw 128k;

      server 127.0.0.1:6379 down;
      server 127.0.0.2:6379 down;
      server 127.0.1.1:6379 down;
      server 127.0.1.2:6379 down;

      check fall=1 rise=2 timeout=10000 interval=10;

      check_request_body "multi\r\ninfo replication\r\nset x check\r\nget x\r\nexec\r\nquit\r\n";
      check_response_body "(\+OK).*(\+QUEUED).*(\+QUEUED).*(\+QUEUED).*(role:master).*(\+OK).*(check).*(\+OK)";
    }
}
```
[Back to TOC](#table-of-contents)


Configuration directives
===============
Global healthcheck parameters may be used in case when you want to cover all upstreams with same healthchecks.
Every parameter may be redefined on `upstream` level.

Individual upstream parameters
--------------------------

check
-----
* **syntax**: `check fall=2 rise=2 timeout=1000 interval=10 keepalive=10 type=http|tcp|ssl`
* **default**: `none`
* **context**: `upstream`

Configure healthcheck base parameters.
If `interval` is 0 - no healthcheks for this upstream.

check_request_uri
-----------------
* **syntax**: `check_request_uri GET|POST /ping`
* **default**: `none`
* **context**: `upstream`

Configure http request for healthcheck.

check_request_headers
-------------------
* **syntax**: `check_request_headers a=1 b=2`
* **default**: `none`
* **context**: `upstream`

Configure http request headers for healthcheck.

check_request_body
-----------------
* **syntax**: `check_request_body hello`
* **default**: `none`
* **context**: `upstream`

Configure http request body for healthcheck.

check_response_codes
-------------------
* **syntax**: `check_response_codes 200 201 202`
* **default**: `none`
* **context**: `upstream`

Configure http response codes for healthcheck.

check_response_body
-------------------
* **syntax**: `check_response_body .*`
* **default**: `none`
* **context**: `upstream`

Configure regular expression for http response body.

[Back to TOC](#table-of-contents)

check_persistent
--------------
* **syntax**: `check_persistent <folder>/off`
* **default**: `off`
* **context**: `upstream`

Configure persistance for healthcheck parameters.

Parameters a stored in plain text files and may be changed online in any time without reloads.
Saved parameters have higher priority than configuration parameters. They are redefine them.

To disable persistace for specific upstream you may write `check_persistent off`.

[Back to TOC](#table-of-contents)

check_disable_host
----------------
* **syntax**: `check_disable_host 1.1.1.1:3456`
* **default**: `none`
* **context**: `upstream`

Disable specific host or peer in upstream (may be changed via persistance).

[Back to TOC](#table-of-contents)

check_exclude_host
----------------
* **syntax**: `check_exclude_host 1.1.1.1:3456`
* **default**: `none`
* **context**: `upstream`

Exclude specific host or peer in upstream from healthcheck.

[Back to TOC](#table-of-contents)

Global healthcheck parameters
--------------------------

healthcheck
----------
* **syntax**: `healthcheck fall=2 rise=2 timeout=1000 interval=10 keepalive=10 type=http|tcp|ssl`
* **default**: `none`
* **context**: `http`

Configure healthcheck base parameters globally.

healthcheck_request_uri
--------------------
* **syntax**: `healthcheck_request_uri GET|POST /ping`
* **default**: `none`
* **context**: `http`

Configure http request for healthcheck globally.

healthcheck_request_headers
------------------------
* **syntax**: `healthcheck_request_headers a=1 b=2`
* **default**: `none`
* **context**: `http`

Configure http request headers for healthcheck globally.

healthcheck_request_body
----------------------
* **syntax**: `healthcheck_request_body hello`
* **default**: `none`
* **context**: `http`

Configure http request body for healthcheck globally.

healthcheck_response_codes
------------------------
* **syntax**: `healthcheck_response_codes 200 201 202`
* **default**: `none`
* **context**: `http`

Configure http response codes for healthcheck globally.

healthcheck_response_body
-----------------------
* **syntax**: `healthcheck_response_body .*`
* **default**: `none`
* **context**: `http`

Configure regular expression for http response body globally.

[Back to TOC](#table-of-contents)

healthcheck_persistent
-------------------
* **syntax**: `healthcheck_persistent <folder>/off`
* **default**: `off`
* **context**: `http`

Configure persistance for healthcheck parameters globally.

Parameters a stored in plain text files and may be changed online in any time without reloads.
Saved parameters have higher priority than configuration parameters. They are redefine them.

[Back to TOC](#table-of-contents)

healthcheck_disable_host
---------------------
* **syntax**: `healthcheck_disable_host 1.1.1.1:4567`
* **default**: `none`
* **context**: `upstream`

Disable specific host or peer in upstream (may be changed via persistance) globally.

healthcheck_buffer_size
--------------------
* **syntax**: `healthcheck_buffer_size 32k`
* **default**: `pagesize`
* **context**: `http`

Specify buffer size for sending and parsing requests and responses.

[Back to TOC](#table-of-contents)

Reconfiguration API
============

healthcheck_get
-------------
* **syntax**: `healthcheck_get;`
* **context**: `location`

Register get healthcheck information handler.

Example output:
`curl localhost:8888/healthcheck/get`
```
{
    "a":{
        "rise":1,
        "fall":2,
        "interval":10,
        "keepalive":10,
        "timeout":10000,
        "type":"http",
        "command":{
            "uri":"/health",
            "method":"GET",
            "headers":{"a":"1","b":"2"},
            "body":"ping",
            "expected":{
                "body":"pong",
                "codes":[200,204,201]
            }
        },
        "disabled":0,
        "off":0,
        "disabled_hosts":[]
    },
    "google":{
        "rise":2,
        "fall":2,
        "interval":10,
        "keepalive":1,
        "timeout":10000,
        "type":"http",
        "command":{
            "uri":"/",
            "method":"GET",
            "headers":[],
            "request_body":"",
            "expected":{
                "body":"",
                "codes":[]
            }
        },
        "disabled":0,
        "off":0,
        "disabled_hosts":[]
    }
}
```

Arguments:

- stream=
- upstream=name

On default the handler returns information about all http upstreams. To get information about streams you may pass `stream=` argument to request.  
To get information about specific upstream you may mass `upstream=xxx` agrument.


[Back to TOC](#table-of-contents)

healthcheck_status
----------------
* **syntax**: `healthcheck_status;`
* **context**: `location`

Get healthcheck online information.

Example output:
`curl localhost:8888/healthcheck/status`
```
{
    "a":{
        "primary":{
            "127.0.0.1:9000":{
                "down":1,
                "fall":0,
                "rise":0,
                "fall_total":0,
                "rise_total":0
            },
            "127.0.1.1:9000":{
                "down":1,
                "fall":0,
                "rise":0,
                "fall_total":0,
                "rise_total":0
            }
        },
        "backup":{
            "127.0.2.1:9000":{
                "down":1,
                "fall":3,
                "rise":0,
                "fall_total":1,
                "rise_total":0
            }
        }
    },
    "google":{
        "primary":{
            "173.194.220.102:80":{
                "down":0,
                "fall":0,
                "rise":2,
                "fall_total":0,
                "rise_total":2
            },
            "173.194.220.138:80":{
                "down":0,
                "fall":0,
                "rise":2,
                "fall_total":0,
                "rise_total":2
            },
            "173.194.220.113:80":{
                "down":0,
                "fall":0,
                "rise":2,
                "fall_total":0,
                "rise_total":2
            },
            "173.194.220.139:80":{
                "down":0,
                "fall":0,
                "rise":2,
                "fall_total":0,
                "rise_total":2
            },
            "173.194.220.100:80":{
                "down":0,
                "fall":0,
                "rise":2,
                "fall_total":0,
                "rise_total":2
            },
            "173.194.220.101:80":{
                "down":0,
                "fall":0,
                "rise":2,
                "fall_total":0,
                "rise_total":2
            }
        }
    }
}
```

Arguments:

- stream=
- upstream=name

On default the handler returns information about all http upstreams. To get information about streams you may pass `stream=` argument to request.  
To get information about specific upstream you may mass `upstream=xxx` agrument.

healthcheck_update
----------------
* **syntax**: `healthcheck_update;`
* **context**: `location`

Arguments:

```
- stream=
- upstream=xxx
- type=http|tcp|ssl
- fall=N
- rise=N
- timeout=ms
- interval=sec
- keepalive=N
- request_uri=URI
- request_method=GET|POST|....
- request_headers=h1:v1|h2:v2|...
- request_body=BODY
- response_codes=200|201|...
- response_body=REGEXP
- off=1|0
- disable_host=XXX.XXX.XXX.XXX:PORT
- enable_host=XXX.XXX.XXX.XXX:PORT
- disable=1|0

```
**upstream** is mandatory for all updates (ex. disable/enable hosts).  
**off** - enable/disable healthchecks for upstream.  
**disable** - absolutely disable app peers in upstream.  
**enable/disable host** may be used with upstream or not. When no upstream is defined peers disabling/enabling in all upstreams.  

[Back to TOC](#table-of-contents)

LUA API
=====

Package `ngx.healthcheck` is used for manipulation http upstreams.
Package `ngx.healthcheck.stream` is used for manipulation stream upstreams.

Functionality of both packages is same.


Methods
=======

lua_get
------
**syntax 1:** `healthcheck, error = hc.get(upstream)`  
**syntax 2:** `healthcheck, error = hc.get()`  
**context:** *&#42;_by_lua&#42;*  

Get healthcheck parameters.

Returns lua table on success, or error otherwise.

[Back to TOC](#table-of-contents)

lua_update
---------
**syntax:** `ok, error = hc.update(upstream, tab)`  
**context:** *&#42;_by_lua&#42;*  

Update healthcheck parameters.

Example table:
```
{
    "rise":1,
    "fall":2,
    "interval":10,
    "keepalive":10,
    "timeout":10000,
    "type":"http",
    "command":{
        "uri":"/health",
        "method":"GET",
        "headers":{"a":"1","b":"2"},
        "body":"ping",
        "expected":{
            "body":"1111",
            "codes":[200,204,201]
        }
    },
    "disabled":0,
    "off":0
}
```

[Back to TOC](#table-of-contents).

lua_disable_host
-------------
**syntax:** `ok, error = hc.disable_host(host, disabled=1|0, [upstream])`  
**context:** *&#42;_by_lua&#42;*  

Disable (permanent down) specific host in upstream (or in all upstreams).

Returns `true` or throws the error.

[Back to TOC](#table-of-contents).

lua_disable
---------
**syntax:** `ok, error = hc.disable(upstream, disabled=1|0)`  
**context:** *&#42;_by_lua&#42;*  

Disable (permanent down) all hosts in upstream.

Returns `true` or throws the error.

[Back to TOC](#table-of-contents).

lua_status
---------
**syntax:** `status, error = hc.status(upstream)`  
**context:** *&#42;_by_lua&#42;*  

Returns runlime healthcheck information about peers in upstream.

[Back to TOC](#table-of-contents).