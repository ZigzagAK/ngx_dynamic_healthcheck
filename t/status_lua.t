use Test::Nginx::Socket;
use Test::Nginx::Socket::Lua::Stream;

repeat_each(1);

plan tests => repeat_each() * 2 * blocks();

run_tests();

__DATA__

=== TEST 1: healthcheck status http lua
--- http_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001 down;
        server 127.0.0.2:6002 down;
        server 127.0.0.3:6003 backup down;
        server 127.0.0.4:6004 backup down;
        server 127.0.0.5:6005 down;
        server 127.0.0.6:6006 backup down;
        check type=http fall=2 rise=1 timeout=1500 interval=1;
        check_request_uri GET /heartbeat;
        check_request_headers aaa=333 bbb=444;
        check_request_body hello;
        check_response_codes 200 201 204;
        check_response_body (primary|backup)\n333\n444\nhello\n;
    }
    upstream u2 {
        zone shm-u2 128k;
        server 127.0.0.1:6001 down;
        server 127.0.0.2:6002 down;
        server 127.0.0.3:6003 backup down;
        server 127.0.0.4:6004 backup down;
        server 127.0.0.5:6005 down;
        server 127.0.0.6:6006 backup down;
        check type=http fall=2 rise=1 timeout=1500 interval=1;
        check_request_uri GET /heartbeat;
        check_request_headers aaa=333 bbb=444;
        check_request_body hello;
        check_response_codes 200 201 204;
        check_response_body (primary|backup)\n333\n444\nhello\n;
    }
    server {
      listen 6001;
      listen 6002;
      location /heartbeat {
        lua_need_request_body on;
        content_by_lua_block {
          ngx.say("primary")
          ngx.say(ngx.var.http_aaa)
          ngx.say(ngx.var.http_bbb)
          ngx.say(ngx.req.get_body_data())
        }
      }
    }
    server {
      listen 6003;
      listen 6004;
      location /heartbeat {
        lua_need_request_body on;
        content_by_lua_block {
          ngx.say("backup")
          ngx.say(ngx.var.http_aaa)
          ngx.say(ngx.var.http_bbb)
          ngx.say(ngx.req.get_body_data())
        }
      }
    }
--- config
    location /test {
        content_by_lua_block {
            ngx.sleep(1)
            local hc = require "ngx.healthcheck"
            ngx.sleep(1)
            local data, err = hc.status()
            if not data then
              ngx.print(err)
            end
            local t = {}
            for u, h in pairs(data)
            do
              for p, s in pairs(h.primary)
              do
                table.insert(t, string.format("%s %s %d 1", u, p, s.down))
              end
              for p, s in pairs(h.backup)
              do
                table.insert(t, string.format("%s %s %d 0", u, p, s.down))
              end
            end
            table.sort(t)
            for i,l in ipairs(t)
            do
              ngx.say(l)
              ngx.log(ngx.INFO, l)
            end
        }
    }
--- request
    GET /test
--- response_body_like
u1 127.0.0.1:6001 0 1
u1 127.0.0.2:6002 0 1
u1 127.0.0.3:6003 0 0
u1 127.0.0.4:6004 0 0
u1 127.0.0.5:6005 1 1
u1 127.0.0.6:6006 1 0
u2 127.0.0.1:6001 0 1
u2 127.0.0.2:6002 0 1
u2 127.0.0.3:6003 0 0
u2 127.0.0.4:6004 0 0
u2 127.0.0.5:6005 1 1
u2 127.0.0.6:6006 1 0


=== TEST 2: healthcheck status stream lua
--- http_config
    server {
      listen 6001;
      listen 6002;
      location /ping {
        content_by_lua_block {
          ngx.say("pong")
        }
      }
    }
    server {
      listen 6003;
      listen 6004;
      location /ping {
        content_by_lua_block {
          ngx.say("pong")
        }
      }
    }
--- stream_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001 down;
        server 127.0.0.2:6002 down;
        server 127.0.0.3:6003 backup down;
        server 127.0.0.4:6004 backup down;
        server 127.0.0.5:6005 down;
        server 127.0.0.6:6006 backup down;
        check fall=2 rise=1 timeout=1500 interval=1;
        check_request_body "GET /ping\r\n\r\n";
        check_response_body pong;
    }
    upstream u2 {
        zone shm-u2 128k;
        server 127.0.0.1:6001 down;
        server 127.0.0.2:6002 down;
        server 127.0.0.3:6003 backup down;
        server 127.0.0.4:6004 backup down;
        server 127.0.0.5:6005 down;
        server 127.0.0.6:6006 backup down;
        check fall=2 rise=1 timeout=1500 interval=1;
        check_request_body "GET /ping\r\n\r\n";
        check_response_body pong;
    }
--- stream_server_config
    proxy_pass u1;
--- config
    location /test {
        content_by_lua_block {
            ngx.sleep(1)
            local hc = require "ngx.healthcheck.stream"
            ngx.sleep(1)
            local data, err = hc.status()
            if not data then
              ngx.print(err)
            end
            local t = {}
            for u, h in pairs(data)
            do
              for p, s in pairs(h.primary)
              do
                table.insert(t, string.format("%s %s %d 1", u, p, s.down))
              end
              for p, s in pairs(h.backup)
              do
                table.insert(t, string.format("%s %s %d 0", u, p, s.down))
              end
            end
            table.sort(t)
            for i,l in ipairs(t)
            do
              ngx.say(l)
              ngx.log(ngx.INFO, l)
            end
        }
    }
--- request
    GET /test
--- response_body_like
u1 127.0.0.1:6001 0 1
u1 127.0.0.2:6002 0 1
u1 127.0.0.3:6003 0 0
u1 127.0.0.4:6004 0 0
u1 127.0.0.5:6005 1 1
u1 127.0.0.6:6006 1 0
u2 127.0.0.1:6001 0 1
u2 127.0.0.2:6002 0 1
u2 127.0.0.3:6003 0 0
u2 127.0.0.4:6004 0 0
u2 127.0.0.5:6005 1 1
u2 127.0.0.6:6006 1 0


