## OpenThread RCP for Pinedio Zigbee Dongle

### Flashing

```bflb-mcu-tool-uart --chipname bl702 --firmware <file>.bin```

### Debug

- Solid White light: Very sad
- Solid Cyan: Also sad
- Breathing Red: No border router service connected
- Blink Red: TX
- Blink Blue: RX

### Settings

UART speed of 115200, no hardware flow control

### Example docker-compose for docker HA
```
otbr:
    container_name: otbr
    image: "openthread/border-router:stable"
    restart: unless-stopped
    ports:
      - 10400:10400
      - 10401:10401
    volumes:
      - /home/user/homeassistant/otbr:/data
    environment:
      - OT_RCP_DEVICE=spinel+hdlc+uart:///dev/ttyUSB0?uart-baudrate=115200
      - OT_THREAD_IF=wpan0
      - OT_INFRA_IF=eth0
      - OT_REST_LISTEN_PORT=10400
      - OT_LOG_LEVEL=1
      - OT_REST_LISTEN_ADDR=0.0.0.0
      - OT_WEB_LISTEN_ADDR=0.0.0.0
      - OT_WEB_LISTEN_PORT=10401
    cap_add:
      - NET_ADMIN
    privileged: true
    devices:
      - "/dev/ttyUSB0:/dev/ttyUSB0"
      - "/dev/net/tun:/dev/net/tun"
    networks:
      default:
        driver_opts:
          com.docker.network.container_iface_prefix: eth0
```
