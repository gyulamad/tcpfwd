### Build wol-proxy:

```
./builder wol-proxy.cpp --mode=strict
```


### Run wol-proxy: (add this to boot startup)

```
.build/strict/wol-proxy <port> <server>:<server-port> <broadcast-ip> <server-mac-address> <ssh-user> <ssh-command>
```

example:

```
.build/strict/wol-proxy 9091 192.168.1.197:8080 192.168.1.255 50:65:F3:2D:45:3F gyula "./llamacpp-server.sh Qwen3 200000"
```

### Server setup

Copy the `auto-shutdown.sh` to the server and call it in the script you will run from the ssh command (which is the `llamacpp-server.sh` in this example).