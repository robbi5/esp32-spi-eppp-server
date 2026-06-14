# ESP32 SPI EPPP Server

An ESP-IDF firmware that turns an ESP32 (such as an ESP32-C3, ESP32-S3 or ESP32-S2) into a WiFi-to-SPI
gateway using Espressif's
[EPPP Link](https://components.espressif.com/components/espressif/eppp_link)
component.

The device acts as an SPI gateway and EPPP server — it waits for an SPI client to connect, then provides that client with NAT-ed IP connectivity over an EPPP tunnel. Any ESP32 with SPI can serve as the client; it only needs to call
`eppp_connect()` to obtain a fully functional network interface.

## How it works

When the ESP32 boots, it tries to connect to your WiFi network. If it doesn't have credentials saved (or if they are incorrect), it starts an open AP called `ESP32-Config` and runs a captive portal on port 80 so you can configure them from a browser.

Once connected to WiFi, it starts the SPI transport and waits for the client. When the client connects, the devices establish a point-to-point IP link. The gateway then forwards the client's traffic to the WiFi network using lwIP's NAPT.

By default, the gateway also runs a port-forwarding proxy. It listens on port 80 on the WiFi side and forwards incoming connections across the SPI link to the client. When the SPI link goes up, the gateway automatically shuts down its WiFi config web server to release port 80 for the proxy.

## Download Release & Flash

Use https://esptool.spacehuhn.com (or any other esp web-usb flashing tool) and flash one of the matching `-merged.bin` for your esp32 [from the releases of this repository](https://github.com/robbi5/esp32-spi-eppp-server/releases/).

## Building it manually

Requires [ESP-IDF](https://github.com/espressif/esp-idf) v6.0 or later.

As some of the dependencies need to be patched (eppp-link for `SPI_ALIGN`ment, esp_bus and esp_wifi_manager 
for ESP-IDF 6.x compatibility), there is the `patches/` folder and the `apply_patches.sh` that downloads the dependencies and patches them.

```sh
./apply_patches.sh
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Configuration

On first boot, connect to the `ESP32-Config` SoftAP and enter your WiFi credentials in the captive portal.

All settings are available via `idf.py menuconfig`:

### WiFi

WiFi credentials are configured at runtime via the captive portal. On first boot the device opens a SoftAP named `ESP32-Config` (open network, no password). Connect to it and your OS will redirect you to the portal where you can scan for and enter your WiFi credentials - if not, try <http://192.168.4.1>. They are saved to NVS and used automatically on subsequent boots.

To switch networks later, use the serial CLI (visible in `idf.py monitor`):

```
eppp> wifi_list              # show saved networks
eppp> wifi_add MyNewSSID password123
eppp> wifi_del OldSSID
eppp> wifi_connect           # reconnect with highest-priority network
eppp> wifi_reset             # clear all saved networks (triggers captive portal on reboot)
```

The AP name and other WiFi manager settings can be tuned via `idf.py menuconfig` → *WiFi Manager*.

### Port forwarding

To make it easy to access a http server running on the SPI client from the WiFi network, the gateway includes a built-in TCP port forwarding proxy (enabled via `CONFIG_EPPP_SRV_TCP_PROXY_ENABLE=y` by default).

All settings can be configured via `idf.py menuconfig` -> *SPI EPPP Server Configuration* -> *TCP port forwarding*:

| Setting | Kconfig key | Default | Description |
|---------|-------------|---------|-------------|
| Enable Proxy | `EPPP_SRV_TCP_PROXY_ENABLE` | `y` | Enable TCP port forwarding to EPPP client |
| Listen Port | `EPPP_SRV_TCP_PROXY_LISTEN_PORT` | `80` | Port to listen on (WiFi side) |
| Client IP | `EPPP_SRV_TCP_PROXY_TARGET_IP` | `"192.168.11.2"` | IP address of the EPPP client |
| Target Port | `EPPP_SRV_TCP_PROXY_TARGET_PORT` | `80` | Target port on EPPP client |


### Pin configuration for ESP32-C3:

https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/spi_master.html#gpio-matrix-and-io-mux

| Setting | Kconfig key | Default | Description |
|---------|-------------|---------|-------------|
| Status LED | `EPPP_SRV_LED_GPIO` | 8 | Status LED GPIO, low-active (-1 to disable) |
| MOSI GPIO | `SPI_EPPP_PIN_MOSI` | 7 | SPI MOSI pin |
| MISO GPIO | `SPI_EPPP_PIN_MISO` | 2 | SPI MISO pin |
| SCLK GPIO | `SPI_EPPP_PIN_SCLK` | 6 | SPI clock pin |
| CS GPIO | `SPI_EPPP_PIN_CS` | 10 | SPI chip select pin |
| INT GPIO | `SPI_EPPP_PIN_INT` | 9 | SPI handshake/interrupt pin |

Pin defaults are for ESP32-C3. Adjust for your board via menuconfig.

### Pin configuration for ESP32-S3 or ESP32-S2:

https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/spi_master.html#gpio-matrix-and-io-mux or https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/api-reference/peripherals/spi_master.html#gpio-matrix-and-io-mux

| Setting | Kconfig key | Default | Description |
|---------|-------------|---------|-------------|
| Status LED | `EPPP_SRV_LED_GPIO` | 8 | Status LED GPIO, low-active (-1 to disable) |
| MOSI GPIO | `SPI_EPPP_PIN_MOSI` | 11 | SPI MOSI pin |
| MISO GPIO | `SPI_EPPP_PIN_MISO` | 13 | SPI MISO pin |
| SCLK GPIO | `SPI_EPPP_PIN_SCLK` | 12 | SPI clock pin |
| CS GPIO | `SPI_EPPP_PIN_CS` | 10 | SPI chip select pin |
| INT GPIO | `SPI_EPPP_PIN_INT` | 9 | SPI handshake/interrupt pin |


## Serial output

The firmware logs a periodic status line (every 10 s):

```
I (36938) EPPP_SRV: [up=36s] eppp=UP wifi=-60dBm ip=192.168.42.123 heap=189184 packets_tx=123 packets_rx=456 packets_err=0
```

## sdkconfig.defaults

The checked-in `sdkconfig.defaults` sets only the essentials:

- Target: `esp32c3`
- Transport: SPI (`CONFIG_EPPP_LINK_DEVICE_SPI`)
- IP forwarding and NAPT enabled
- VJ header compression disabled (not needed for local SPI link)

Pin assignments are **not** in `sdkconfig.defaults`
and must be configured via `idf.py menuconfig` (or use one of the provided sdkconfig.esp32* files)

## Thanks

This project is based on https://github.com/hn/esp32-spi-eppp-server
