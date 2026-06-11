# ESP32 SPI EPPP Server

An ESP-IDF firmware that turns an ESP32 (e.g. ESP32-C3) into a WiFi-to-SPI
gateway using Espressif's
[EPPP Link](https://components.espressif.com/components/espressif/eppp_link)
component.

The device acts as an SPI slave and EPPP server — it waits for an SPI master
to connect, then provides that host with NAT-ed IP connectivity over an EPPP
tunnel. Any ESP32 with SPI can serve as the host; it only needs to call
`eppp_connect()` to obtain a fully functional network interface.

## How it works

1. The server starts a WiFi manager. On first boot (or when saved credentials are erased), it opens a SoftAP captive portal (`ESP32-Config` by default) so you can enter WiFi credentials via a browser. Credentials are stored in NVS and reused on subsequent boots.
2. It starts an EPPP SPI slave and waits for a host (SPI master) to connect.
3. Once connected, an IP tunnel is established over SPI and traffic is
   forwarded between the EPPP interface and WiFi using lwIP NAT (NAPT).

## Building

Requires [ESP-IDF](https://github.com/espressif/esp-idf) v6.0 or later.

```sh
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

On first boot, connect to the `ESP32-Config` SoftAP and enter your WiFi credentials in the captive portal.

## Configuration

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

### For ESP32-C3:

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

### For ESP32-S3 or ESP32-S2:

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
I (36938) EPPP_SRV: [up=36s] eppp=UP wifi=-60dBm ip=192.168.42.123 heap=189184
```

## sdkconfig.defaults

The checked-in `sdkconfig.defaults` sets only the essentials:

- Target: `esp32c3`
- Transport: SPI (`CONFIG_EPPP_LINK_DEVICE_SPI`)
- IP forwarding and NAPT enabled
- VJ header compression disabled (not needed for local SPI link)

Pin assignments are **not** in `sdkconfig.defaults`
and must be configured via `idf.py menuconfig`.

## Thanks

This project is based on https://github.com/hn/esp32-spi-eppp-server
