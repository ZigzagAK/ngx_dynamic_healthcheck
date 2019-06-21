use Test::Nginx::Socket;
use Test::Nginx::Socket::Lua::Stream;

repeat_each(1);

plan tests => repeat_each() * 2 * blocks();

run_tests();

__DATA__

=== TEST 1: check exclude_host http
--- http_config
    lua_load_resty_core off;
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001 down;
        server unix:/tmp/u1.sock down;
        check type=http fall=2 rise=1 timeout=1500 interval=1;
        check_request_uri GET /heartbeat;
        check_response_codes 200 201 204;
    }
    upstream u2 {
        zone shm-u2 128k;
        server 127.0.0.1:6001 down;
        server unix:/tmp/u1.sock;
        check type=http fall=2 rise=1 timeout=1500 interval=1;
        check_request_uri GET /heartbeat;
        check_response_codes 200 201 204;
        check_exclude_host unix:logs/u1.sock;
    }
    upstream u3 {
        zone shm-u3 128k;
        server 127.0.0.1:6001 down;
        server unix:/tmp/u1.sock down;
        check type=http fall=2 rise=1 timeout=1500 interval=1;
        check_request_uri GET /heartbeat;
        check_response_body xxx;
        check_response_codes 200 201 204;
    }
    server {
      listen 6001;
      listen unix:/tmp/u1.sock;
      location /heartbeat {
        content_by_lua_block {
          ngx.say("pong")
        }
      }
    }
--- config
    location /status {
      healthcheck_status;
    }
    location /test {
        content_by_lua_block {
            ngx.sleep(1)
            local resp = assert(ngx.location.capture("/status"))
            if resp.status ~= ngx.HTTP_OK then
              ngx.say(resp.status)
            end
            local cjson = require "cjson"
            local data = cjson.decode(resp.body)
            local t = {}
            for u, h in pairs(data)
            do
              for p, s in pairs(h.primary)
              do
                table.insert(t, string.format("%s %s %d 1", u, p, s.down))
              end
              for p, s in pairs(h.backup or {})
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
u1 unix:/tmp/u1.sock 0 1
u2 127.0.0.1:6001 0 1
u2 unix:/tmp/u1.sock 0 1
u3 127.0.0.1:6001 1 1
u3 unix:/tmp/u1.sock 1 1


=== TEST 2: check exclude_host stream
--- stream_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001 down;
        server unix:/tmp/u1.sock down;
        check fall=2 rise=1 timeout=1500 interval=1;
        check_request_body "GET /heartbeat HTTP/1.0\r\nConnection: close;\r\n\r\n";
        check_response_body pong;
    }
    upstream u2 {
        zone shm-u2 128k;
        server 127.0.0.1:6001 down;
        server unix:/tmp/u1.sock;
        check fall=2 rise=1 timeout=1500 interval=1;
        check_request_body "GET /heartbeat HTTP/1.0\r\nConnection: close;\r\n\r\n";
        check_response_body pong;
        check_exclude_host unix:logs/u1.sock;
    }
    upstream u3 {
        zone shm-u3 128k;
        server 127.0.0.1:6001 down;
        server unix:/tmp/u1.sock down;
        check fall=2 rise=1 timeout=1500 interval=1;
        check_request_body "GET /heartbeat HTTP/1.0\r\nConnection: close;\r\n\r\n";
        check_response_body xxx;
    }
--- stream_server_config
    proxy_pass u1;
--- http_config
    lua_load_resty_core off;
    server {
      listen 6001;
      listen unix:/tmp/u1.sock;
      location /heartbeat {
        content_by_lua_block {
          ngx.say("pong")
        }
      }
    }
--- config
    location /status {
      healthcheck_status;
    }
    location /test {
        content_by_lua_block {
            ngx.sleep(1)
            local resp = assert(ngx.location.capture("/status?stream="))
            if resp.status ~= ngx.HTTP_OK then
              ngx.say(resp.status)
            end
            local cjson = require "cjson"
            local data = cjson.decode(resp.body)
            local t = {}
            for u, h in pairs(data)
            do
              for p, s in pairs(h.primary)
              do
                table.insert(t, string.format("%s %s %d 1", u, p, s.down))
              end
              for p, s in pairs(h.backup or {})
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
u1 unix:/tmp/u1.sock 0 1
u2 127.0.0.1:6001 0 1
u2 unix:/tmp/u1.sock 0 1
u3 127.0.0.1:6001 1 1
u3 unix:/tmp/u1.sock 1 1
