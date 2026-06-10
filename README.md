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

1. The server connects to a WiFi access point as a station.
2. It starts an EPPP SPI slave and waits for a host (SPI master) to connect.
3. Once connected, an IP tunnel is established over SPI and traffic is
   forwarded between the EPPP interface and WiFi using lwIP NAT (NAPT).

## Building

Requires [ESP-IDF](https://github.com/espressif/esp-idf) v6.0 or later.

```sh
# Set WiFi credentials (stored in sdkconfig, not in sdkconfig.defaults)
idf.py menuconfig
# → SPI EPPP Server Configuration → WiFi SSID / WiFi Password

idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Configuration

All settings are available via `idf.py menuconfig`:


| Setting | Kconfig key | Default | Description |
|---------|-------------|---------|-------------|
| WiFi SSID | `EPPP_SRV_WIFI_SSID` | *(empty)* | SSID of the WiFi network to join |
| WiFi Password | `EPPP_SRV_WIFI_PASSWORD` | *(empty)* | WiFi password |

Pin defaults are for ESP32-C3. Adjust for your board via menuconfig.

### For ESP32-C3:

| Setting | Kconfig key | Default | Description |
|---------|-------------|---------|-------------|
| WiFi SSID | `EPPP_SRV_WIFI_SSID` | *(empty)* | SSID of the WiFi network to join |
| WiFi Password | `EPPP_SRV_WIFI_PASSWORD` | *(empty)* | WiFi password |
| Status LED | `EPPP_SRV_LED_GPIO` | 8 | Status LED GPIO, low-active (-1 to disable) |
| MOSI GPIO | `SPI_EPPP_PIN_MOSI` | 6 | SPI MOSI pin |
| MISO GPIO | `SPI_EPPP_PIN_MISO` | 5 | SPI MISO pin |
| SCLK GPIO | `SPI_EPPP_PIN_SCLK` | 4 | SPI clock pin |
| CS GPIO | `SPI_EPPP_PIN_CS` | 7 | SPI chip select pin |
| INT GPIO | `SPI_EPPP_PIN_INT` | 10 | SPI handshake/interrupt pin |

Pin defaults are for ESP32-C3. Adjust for your board via menuconfig.

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

WiFi credentials and pin assignments are **not** in `sdkconfig.defaults`
and must be configured via `idf.py menuconfig`.
