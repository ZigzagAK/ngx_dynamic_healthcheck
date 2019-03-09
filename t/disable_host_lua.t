use Test::Nginx::Socket;
use Test::Nginx::Socket::Lua::Stream;

repeat_each(1);

plan tests => repeat_each() * 2 * blocks();

run_tests();

__DATA__


=== TEST 1: healthcheck status http lua disable/enable
--- http_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001 down;
        server 127.0.0.2:6002 backup down;
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
--- config
    location /test {
        content_by_lua_block {
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
                table.insert(t, string.format("%s %s %d 1", u, p, s.disabled and 1 or 0))
              end
              for p, s in pairs(h.backup)
              do
                table.insert(t, string.format("%s %s %d 0", u, p, s.disabled and 1 or 0))
              end
            end
            table.sort(t)
            for i,l in ipairs(t)
            do
              ngx.say(l)
              ngx.log(ngx.INFO, l)
            end
            local cjson = require "cjson"
            -- disable
            hc.disable_host("127.0.0.1:6001", true, "u1")
            hc.disable_host("127.0.0.2:6002", true, "u1")
            local data, err = hc.get()
            if not data then
              ngx.print(err)
            end
            for i,h in ipairs(data["u1"].disabled_hosts)
            do
              ngx.say(h)
            end
            ngx.log(ngx.INFO, cjson.encode(data))
            -- enable
            hc.disable_host("127.0.0.1:6001", false, "u1")
            hc.disable_host("127.0.0.2:6002", false, "u1")
            local data, err = hc.get()
            if not data then
              ngx.print(err)
            end
            if #data["u1"].disabled_hosts == 0 then
              ngx.say("-")
            else
              for i,h in ipairs(data["u1"].disabled_hosts)
              do
                ngx.say(h)
              end
            end
            ngx.log(ngx.INFO, cjson.encode(data))
        }
    }
--- request
    GET /test
--- response_body_like
u1 127.0.0.1:6001 0 1
u1 127.0.0.2:6002 0 0
127.0.0.1:6001
127.0.0.2:6002
-


=== TEST 2: healthcheck status http lua disable/enable in all upstreams
--- http_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001 down;
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
--- config
    location /test {
        content_by_lua_block {
            local hc = require "ngx.healthcheck"
            ngx.sleep(1)
            local cjson = require "cjson"
            -- disable
            local ok, err = hc.disable_host("127.0.0.1:6001", true)
            if not ok then
              ngx.print(err)
              return
            end
            local data, err = hc.get()
            if not data then
              ngx.print(err)
            end
            for i,h in ipairs(data["u1"].disabled_hosts)
            do
                ngx.print("u1 ")
                ngx.say(h)
            end
            for i,h in ipairs(data["u2"].disabled_hosts)
            do
                ngx.print("u2 ")
                ngx.say(h)
            end
            ngx.log(ngx.INFO, cjson.encode(data))
            -- enable
            hc.disable_host("127.0.0.1:6001", false)
            local data, err = hc.get()
            if not data then
              ngx.print(err)
            end
            if #data["u1"].disabled_hosts == 0 then
              ngx.say("-")
            else
              for i,h in ipairs(data["u1"].disabled_hosts)
              do
                ngx.print("u1 ")
                ngx.say(h)
              end
            end
            if #data["u2"].disabled_hosts == 0 then
              ngx.say("-")
            else
              for i,h in ipairs(data["u2"].disabled_hosts)
              do
                ngx.print("u2 ")
                ngx.say(h)
              end
            end
            ngx.log(ngx.INFO, cjson.encode(data))
        }
    }
--- request
    GET /test
--- response_body_like
u1 127.0.0.1:6001
u2 127.0.0.1:6001
-
-


=== TEST 3: healthcheck status http lua disable/enable upstream
--- http_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001 down;
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
--- config
    location /test {
        content_by_lua_block {
            local hc = require "ngx.healthcheck"
            ngx.sleep(1)
            local cjson = require "cjson"
            -- disable
            hc.disable("u1", true)
            hc.disable("u2", true)
            local data, err = hc.get()
            if not data then
              ngx.print(err)
            end
            ngx.print("u1 ")
            ngx.say(data["u1"].disabled);
            ngx.print("u2 ")
            ngx.say(data["u2"].disabled);
            ngx.log(ngx.INFO, cjson.encode(data))
            -- enable
            hc.disable("u1", false)
            hc.disable("u2", false)
            local data, err = hc.get()
            if not data then
              ngx.print(err)
            end
            ngx.print("u1 ")
            ngx.say(data["u1"].disabled);
            ngx.print("u2 ")
            ngx.say(data["u2"].disabled);
            ngx.log(ngx.INFO, cjson.encode(data))
        }
    }
--- request
    GET /test
--- response_body_like
u1 1
u2 1
u1 0
u2 0


=== TEST 4: healthcheck status stream lua disable/enable
--- stream_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001 down;
        server 127.0.0.2:6002 backup down;
        check fall=2 rise=1 timeout=1500 interval=1;
        check_request_body "GET /heartbeat\r\n\r\n";
        check_response_body pong;
    }
--- http_config
    server {
      listen 6001;
      listen 6002;
      location /heartbeat {
        lua_need_request_body on;
        content_by_lua_block {
          ngx.say("pong")
        }
      }
    }
--- stream_server_config
    proxy_pass u1;
--- config
    location /test {
        content_by_lua_block {
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
                table.insert(t, string.format("%s %s %d 1", u, p, s.disabled and 1 or 0))
              end
              for p, s in pairs(h.backup)
              do
                table.insert(t, string.format("%s %s %d 0", u, p, s.disabled and 1 or 0))
              end
            end
            table.sort(t)
            for i,l in ipairs(t)
            do
              ngx.say(l)
              ngx.log(ngx.INFO, l)
            end
            local cjson = require "cjson"
            -- disable
            hc.disable_host("127.0.0.1:6001", true, "u1")
            hc.disable_host("127.0.0.2:6002", true, "u1")
            local data, err = hc.get()
            if not data then
              ngx.print(err)
            end
            for i,h in ipairs(data["u1"].disabled_hosts)
            do
              ngx.say(h)
            end
            ngx.log(ngx.INFO, cjson.encode(data))
            -- enable
            hc.disable_host("127.0.0.1:6001", false, "u1")
            hc.disable_host("127.0.0.2:6002", false, "u1")
            local data, err = hc.get()
            if not data then
              ngx.print(err)
            end
            if #data["u1"].disabled_hosts == 0 then
              ngx.say("-")
            else
              for i,h in ipairs(data["u1"].disabled_hosts)
              do
                ngx.say(h)
              end
            end
            ngx.log(ngx.INFO, cjson.encode(data))
        }
    }
--- request
    GET /test
--- response_body_like
u1 127.0.0.1:6001 0 1
u1 127.0.0.2:6002 0 0
127.0.0.1:6001
127.0.0.2:6002
-


=== TEST 5: healthcheck status stream lua disable/enable in all upstreams
--- stream_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001 down;
        check fall=2 rise=1 timeout=1500 interval=1;
        check_request_body "GET /heartbeat\r\n\r\n";
        check_response_body pong;
    }
    upstream u2 {
        zone shm-u2 128k;
        server 127.0.0.1:6001 down;
        check fall=2 rise=1 timeout=1500 interval=1;
        check_request_body "GET /heartbeat\r\n\r\n";
        check_response_body pong;
    }
--- http_config
    server {
      listen 6001;
      listen 6002;
      location /heartbeat {
        lua_need_request_body on;
        content_by_lua_block {
          ngx.say("pong")
        }
      }
    }
--- stream_server_config
    proxy_pass u1;
--- config
    location /test {
        content_by_lua_block {
            local hc = require "ngx.healthcheck.stream"
            ngx.sleep(1)
            local cjson = require "cjson"
            -- disable
            local ok, err = hc.disable_host("127.0.0.1:6001", true)
            if not ok then
              ngx.print(err)
              return
            end
            local data, err = hc.get()
            if not data then
              ngx.print(err)
            end
            for i,h in ipairs(data["u1"].disabled_hosts)
            do
                ngx.print("u1 ")
                ngx.say(h)
            end
            for i,h in ipairs(data["u2"].disabled_hosts)
            do
                ngx.print("u2 ")
                ngx.say(h)
            end
            ngx.log(ngx.INFO, cjson.encode(data))
            -- enable
            hc.disable_host("127.0.0.1:6001", false)
            local data, err = hc.get()
            if not data then
              ngx.print(err)
            end
            if #data["u1"].disabled_hosts == 0 then
              ngx.say("-")
            else
              for i,h in ipairs(data["u1"].disabled_hosts)
              do
                ngx.print("u1 ")
                ngx.say(h)
              end
            end
            if #data["u2"].disabled_hosts == 0 then
              ngx.say("-")
            else
              for i,h in ipairs(data["u2"].disabled_hosts)
              do
                ngx.print("u2 ")
                ngx.say(h)
              end
            end
            ngx.log(ngx.INFO, cjson.encode(data))
        }
    }
--- request
    GET /test
--- response_body_like
u1 127.0.0.1:6001
u2 127.0.0.1:6001
-
-


=== TEST 6: healthcheck status stream lua disable/enable upstream
--- stream_config
    upstream u1 {
        zone shm-u1 128k;
        server 127.0.0.1:6001 down;
        check fall=2 rise=1 timeout=1500 interval=1;
        check_request_body "GET /heartbeat\r\n\r\n";
        check_response_body pong;
    }
    upstream u2 {
        zone shm-u2 128k;
        server 127.0.0.1:6001 down;
        check fall=2 rise=1 timeout=1500 interval=1;
        check_request_body "GET /heartbeat\r\n\r\n";
        check_response_body pong;
    }
--- http_config
    server {
      listen 6001;
      listen 6002;
      location /heartbeat {
        lua_need_request_body on;
        content_by_lua_block {
          ngx.say("pong")
        }
      }
    }
--- stream_server_config
    proxy_pass u1;
--- config
    location /test {
        content_by_lua_block {
            local hc = require "ngx.healthcheck.stream"
            ngx.sleep(1)
            local cjson = require "cjson"
            -- disable
            hc.disable("u1", true)
            hc.disable("u2", true)
            local data, err = hc.get()
            if not data then
              ngx.print(err)
            end
            ngx.print("u1 ")
            ngx.say(data["u1"].disabled);
            ngx.print("u2 ")
            ngx.say(data["u2"].disabled);
            ngx.log(ngx.INFO, cjson.encode(data))
            -- enable
            hc.disable("u1", false)
            hc.disable("u2", false)
            local data, err = hc.get()
            if not data then
              ngx.print(err)
            end
            ngx.print("u1 ")
            ngx.say(data["u1"].disabled);
            ngx.print("u2 ")
            ngx.say(data["u2"].disabled);
            ngx.log(ngx.INFO, cjson.encode(data))
        }
    }
--- request
    GET /test
--- response_body_like
u1 1
u2 1
u1 0
u2 0
