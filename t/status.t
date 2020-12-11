use Test::Nginx::Socket;
use Test::Nginx::Socket::Lua::Stream;

repeat_each(1);

plan tests => repeat_each() * 2 * blocks();

run_tests();

__DATA__

=== TEST 1: healthcheck status http
--- http_config
    lua_load_resty_core off;
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
--- timeout: 3
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


=== TEST 2: healthcheck status stream
--- http_config
    lua_load_resty_core off;
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
--- timeout: 3
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


=== TEST 3: check http HTTP/1.0
--- stream_config
    server {
      listen 6001;
      content_by_lua_block {
        ngx.print("HTTP/1.0 200 OK\r\n")
        ngx.print("Server: nginx/1.13.12\r\n")
        ngx.print("Content-Type: text/plain\r\n")
        ngx.print("Connection: close\r\n\r\n")
        ngx.print("pong\r\n")
      }
    }
--- stream_server_config
    content_by_lua_block {
      ngx.say("hello")
    }
--- http_config
    lua_load_resty_core off;
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001 down;
        check type=http fall=2 rise=1 timeout=1500 interval=1;
        check_request_uri GET /heartbeat;
        check_response_body pong;
        check_response_codes 200 201 204;
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
--- timeout: 3
--- request
    GET /test
--- response_body_like
u1 127.0.0.1:6001 0 1


=== TEST 4: healthcheck status stream HTTP
--- http_config
    lua_load_resty_core off;
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
--- stream_config
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
--- stream_server_config
    proxy_pass u1;
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
--- timeout: 3
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


=== TEST 5: healthcheck status stream HTTP another port
--- http_config
    lua_load_resty_core off;
    server {
      listen 6001;
      location /heartbeat {
        lua_need_request_body on;
        content_by_lua_block {
          ngx.say(ngx.var.http_aaa)
          ngx.say(ngx.var.http_bbb)
          ngx.say(ngx.req.get_body_data())
        }
      }
    }
--- stream_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6000 down;
        check type=http fall=2 rise=1 timeout=1500 interval=1 port=6001;
        check_request_uri GET /heartbeat;
        check_request_headers aaa=333 bbb=444;
        check_request_body hello;
        check_response_codes 200 201 204;
        check_response_body 333\n444\nhello\n;
    }
--- stream_server_config
    proxy_pass u1;
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
                table.insert(t, string.format("%s %s %d", u, p, s.down))
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
u1 127.0.0.1:6000 0


=== TEST 6: check http close socket
--- stream_config
    server {
      listen 6001;
      content_by_lua_block {
        ngx.sleep(0.5)
      }
    }
--- stream_server_config
    content_by_lua_block {
      ngx.say("hello")
    }
--- http_config
    lua_load_resty_core off;
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001 down;
        server 127.0.0.2:6001 down backup;
        check type=http fall=2 rise=1 timeout=1500 interval=1;
        check_request_uri GET /heartbeat;
        check_response_body pong;
        check_response_codes 200 201 204;
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
                table.insert(t, string.format("%s %s %d 1 %d %d", u, p, s.down, s.fall, s.rise))
              end
              for p, s in pairs(h.backup or {})
              do
                table.insert(t, string.format("%s %s %d 0 %d %d", u, p, s.down, s.fall, s.rise))
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
--- timeout: 3
--- request
    GET /test
--- response_body
u1 127.0.0.1:6001 1 1 1 0
u1 127.0.0.2:6001 1 0 1 0


=== TEST 7: check http close socket on body
--- stream_config
    server {
      listen 6001;
      content_by_lua_block {
        ngx.print("HTTP/1.0 200 OK\r\n")
        ngx.print("Server: nginx/1.13.12\r\n")
        ngx.print("Content-Type: text/plain\r\n")
        ngx.print("Content-Length: 10\r\n")
        ngx.print("Connection: close\r\n\r\n")
      }
    }
--- stream_server_config
    content_by_lua_block {
      ngx.say("hello")
    }
--- http_config
    lua_load_resty_core off;
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001 down;
        server 127.0.0.2:6001 down backup;
        check type=http fall=2 rise=1 timeout=1500 interval=1;
        check_request_uri GET /heartbeat;
        check_response_body pong;
        check_response_codes 200 201 204;
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
                table.insert(t, string.format("%s %s %d 1 %d %d", u, p, s.down, s.fall, s.rise))
              end
              for p, s in pairs(h.backup or {})
              do
                table.insert(t, string.format("%s %s %d 0 %d %d", u, p, s.down, s.fall, s.rise))
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
--- timeout: 3
--- request
    GET /test
--- response_body_like
u1 127.0.0.1:6001 1 1 1 0
u1 127.0.0.2:6001 1 0 1 0


=== TEST 8: check http body < content-length
--- stream_config
    server {
      listen 6001;
      content_by_lua_block {
        ngx.print("HTTP/1.0 200 OK\r\n")
        ngx.print("Server: nginx/1.13.12\r\n")
        ngx.print("Content-Type: text/plain\r\n")
        ngx.print("Content-Length: 100\r\n")
        ngx.print("Connection: close\r\n\r\n")
        ngx.print("pong\r\n")
      }
    }
--- stream_server_config
    content_by_lua_block {
      ngx.say("hello")
    }
--- http_config
    lua_load_resty_core off;
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001 down;
        server 127.0.0.2:6001 down backup;
        check type=http fall=2 rise=1 timeout=1500 interval=1;
        check_request_uri GET /heartbeat;
        check_response_body pong;
        check_response_codes 200 201 204;
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
                table.insert(t, string.format("%s %s %d 1 %d %d", u, p, s.down, s.fall, s.rise))
              end
              for p, s in pairs(h.backup or {})
              do
                table.insert(t, string.format("%s %s %d 0 %d %d", u, p, s.down, s.fall, s.rise))
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
--- timeout: 3
--- request
    GET /test
--- response_body_like
u1 127.0.0.1:6001 1 1 1 0
u1 127.0.0.2:6001 1 0 1 0


=== TEST 9: healthcheck chunked
--- http_config
    lua_load_resty_core off;
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001 down;
        check type=http fall=2 rise=1 timeout=5000 interval=1 keepalive=10;
        check_request_uri GET /heartbeat;
        check_request_headers aaa=1111111111 bbb=2222222222 ccc=3333333333;
        check_response_codes 200 201 204;
        check_response_body 1111111111\n2222222222\n3333333333\n;
    }
    server {
      listen 6001;
      keepalive_timeout  600s;
      keepalive_requests 10000;
      location /heartbeat {
        lua_need_request_body on;
        content_by_lua_block {
          ngx.say(ngx.var.http_aaa)
          ngx.flush(true)
          ngx.sleep(1)
          ngx.say(ngx.var.http_bbb)
          ngx.flush(true)
          ngx.sleep(1)
          ngx.say(ngx.var.http_ccc)
          ngx.flush(true)
        }
      }
    }
--- config
    location /status {
      healthcheck_status;
    }
    location /test {
        content_by_lua_block {
            ngx.sleep(2.5)
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
            end
            table.sort(t)
            for i,l in ipairs(t)
            do
              ngx.say(l)
              ngx.log(ngx.INFO, l)
            end
        }
    }
--- timeout: 3
--- request
    GET /test
--- timeout: 3
--- response_body_like
u1 127.0.0.1:6001 0 1


=== TEST 10: check http content-length = 0
--- stream_config
    server {
      listen 6001;
      content_by_lua_block {
        ngx.print("HTTP/1.0 200 OK\r\n")
        ngx.print("Server: nginx/1.13.12\r\n")
        ngx.print("Content-Type: text/plain\r\n")
        ngx.print("Content-Length: 0\r\n")
        ngx.print("Connection: close\r\n\r\n")
      }
    }
--- stream_server_config
    content_by_lua_block {
      ngx.say("hello")
    }
--- http_config
    lua_load_resty_core off;
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001 down;
        server 127.0.0.2:6001 down backup;
        check type=http fall=2 rise=1 timeout=1500 interval=1;
        check_request_uri GET /heartbeat;
        check_response_codes 200 201 204;
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
                table.insert(t, string.format("%s %s %d 1 %d %d", u, p, s.down, s.fall, s.rise))
              end
              for p, s in pairs(h.backup or {})
              do
                table.insert(t, string.format("%s %s %d 0 %d %d", u, p, s.down, s.fall, s.rise))
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
--- timeout: 3
--- request
    GET /test
--- response_body_like
u1 127.0.0.1:6001 0 1 0 1
u1 127.0.0.2:6001 0 0 0 1


=== TEST 11: healthcheck Host header
--- http_config
    lua_load_resty_core off;
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001 down;
        check type=http fall=2 rise=1 timeout=1500 interval=60;
        check_request_uri GET /heartbeat;
        check_request_headers Host=test.com;
        check_response_codes 200;
        check_response_body test.com;
    }
    upstream u2 {
        zone shm-u2 128k;
        server 127.0.0.1:6002 down;
        check type=http fall=2 rise=1 timeout=1500 interval=60;
        check_request_uri GET /heartbeat;
        check_response_codes 200;
        check_response_body 127.0.0.1:6002;
    }
    server {
        listen 6001;
        location /heartbeat {
            echo $http_host;
        }
    }
    server {
        listen 6002;
        location /heartbeat {
            echo $http_host;
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
                table.insert(t, string.format("%s %s %d", u, p, s.down))
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
--- timeout: 3
--- request
    GET /test
--- response_body_like
u1 127.0.0.1:6001 0
u2 127.0.0.1:6002 0


=== TEST 12: check http NO_CONTENT
--- stream_config
    server {
      listen 6001;
      content_by_lua_block {
        ngx.print("HTTP/1.0 204 No Content\r\n")
        ngx.print("Server: nginx/1.13.12\r\n")
        ngx.print("Connection: close\r\n\r\n")
      }
    }
--- stream_server_config
    content_by_lua_block {
      ngx.say("hello")
    }
--- http_config
    lua_load_resty_core off;
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001 down;
        server 127.0.0.2:6001 down backup;
        check type=http fall=2 rise=1 timeout=1500 interval=1;
        check_request_uri GET /heartbeat;
        check_response_codes 200 201 204;
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
                table.insert(t, string.format("%s %s %d 1 %d %d", u, p, s.down, s.fall, s.rise))
              end
              for p, s in pairs(h.backup or {})
              do
                table.insert(t, string.format("%s %s %d 0 %d %d", u, p, s.down, s.fall, s.rise))
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
--- timeout: 3
--- request
    GET /test
--- response_body_like
u1 127.0.0.1:6001 0 1 0 1
u1 127.0.0.2:6001 0 0 0 1


=== TEST 13: check http Chunked 2
--- stream_config
    server {
      listen 6001;
      content_by_lua_block {
        ngx.print("HTTP/1.0 200 OK\r\n")
        ngx.print("Server: nginx/1.13.12\r\n")
        ngx.print("Transfer-Encoding: chunked\r\n\r\n")
        ngx.print("4\r\n")
        ngx.print("Wiki\r\n")
        ngx.print("5\r\n")
        ngx.print("pedia\r\n")
        ngx.flush(true)
        ngx.sleep(0.5)
        ngx.print("3\r\n")
        ngx.print(" in\r\n")
        ngx.print("8\r\n")
        ngx.flush(true)
        ngx.sleep(0.5)
        ngx.print(" chunks.\r\n")
        ngx.print("0\r\n")
        ngx.print("\r\n")
      }
    }
--- stream_server_config
    content_by_lua_block {
      ngx.say("hello")
    }
--- http_config
    lua_load_resty_core off;
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001 down;
        check type=http fall=2 rise=1 timeout=1500 interval=1;
        check_request_uri GET /heartbeat;
        check_response_codes 200 201 204;
        check_response_body "Wikipedia in chunks.";
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
                table.insert(t, string.format("%s %s %d 1 %d %d", u, p, s.down, s.fall, s.rise))
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
--- timeout: 3
--- request
    GET /test
--- response_body_like
u1 127.0.0.1:6001 0 1 0 1


=== TEST 24: healthcheck chunked (no body)
--- stream_config
    server {
      listen 6001;
      content_by_lua_block {
        ngx.print("HTTP/1.0 200 OK\r\n")
        ngx.print("Server: nginx/1.13.12\r\n")
        ngx.print("Transfer-Encoding: chunked\r\n\r\n")
      }
    }
--- stream_server_config
    content_by_lua_block {
      ngx.say("hello")
    }
--- http_config
    lua_load_resty_core off;
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001 down;
        check type=http fall=2 rise=1 timeout=5000 interval=1 keepalive=10;
        check_request_uri GET /heartbeat;
        check_request_headers aaa=1111111111 bbb=2222222222 ccc=3333333333;
        check_response_codes 200 201 204;
        check_response_body 1111111111\n2222222222\n3333333333\n;
    }
--- config
    location /status {
      healthcheck_status;
    }
    location /test {
        content_by_lua_block {
            ngx.sleep(2.5)
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
            end
            table.sort(t)
            for i,l in ipairs(t)
            do
              ngx.say(l)
              ngx.log(ngx.INFO, l)
            end
        }
    }
--- timeout: 3
--- request
    GET /test
--- timeout: 3
--- response_body_like
u1 127.0.0.1:6001 1 1
