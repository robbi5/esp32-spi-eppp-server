#include "sdkconfig.h"

#include <string.h>
#include <stdlib.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "eppp_link.h"
#include "esp_wifi_manager.h"
#include "esp_bus.h"
#include "esp_console.h"
#include "esp_http_server.h"

static const char TAG[] = "EPPP_SRV";

static void init_console(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "eppp>";
    repl_config.max_cmdline_length = 256;
    esp_console_register_help_command();

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_console_dev_usb_serial_jtag_config_t hw_config =
        ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));
#elif CONFIG_ESP_CONSOLE_UART
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));
#endif

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

static void eppp_perform_task(void *arg)
{
    esp_netif_t *netif = (esp_netif_t *)arg;
    while (eppp_perform(netif) != ESP_ERR_TIMEOUT) {}
    vTaskDelete(NULL);
}

static void eppp_server_status_task(void *arg)
{
    esp_netif_t *eppp_netif = (esp_netif_t *)arg;
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10000));

        uint32_t uptime = (uint32_t)(esp_timer_get_time() / 1000000);
        bool eppp_up = esp_netif_is_netif_up(eppp_netif);
        uint32_t heap = esp_get_free_heap_size();

        /* WiFi RSSI */
        int rssi = 0;
        bool wifi_connected = false;
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
        {
            rssi = ap_info.rssi;
            wifi_connected = true;
        }

#if CONFIG_EPPP_SRV_LED_GPIO >= 0
        gpio_set_level(CONFIG_EPPP_SRV_LED_GPIO, wifi_connected ? 0 : 1);
#endif

        /* WiFi IP */
        esp_netif_ip_info_t ip_info = {0};
        if (sta_netif)
            esp_netif_get_ip_info(sta_netif, &ip_info);

        ESP_LOGI(TAG, "[up=%lus] eppp=%s wifi=%ddBm ip=" IPSTR " heap=%lu",
                 uptime,
                 eppp_up ? "UP" : "DOWN",
                 rssi,
                 IP2STR(&ip_info.ip),
                 heap);
    }
}

#if CONFIG_EPPP_SRV_TCP_PROXY_ENABLE

#define PROXY_BUF_SIZE 1460

typedef struct {
    int client_fd;
} proxy_conn_t;

static void proxy_forward_task(void *arg)
{
    proxy_conn_t *conn = (proxy_conn_t *)arg;
    int client_fd = conn->client_fd;
    free(conn);

    int backend_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (backend_fd < 0) {
        ESP_LOGW(TAG, "proxy: socket() failed");
        close(client_fd);
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in dest = {
        .sin_family = AF_INET,
        .sin_port   = htons(CONFIG_EPPP_SRV_TCP_PROXY_TARGET_PORT),
    };
    inet_pton(AF_INET, CONFIG_EPPP_SRV_TCP_PROXY_TARGET_IP, &dest.sin_addr);

    if (connect(backend_fd, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        ESP_LOGW(TAG, "proxy: connect to " CONFIG_EPPP_SRV_TCP_PROXY_TARGET_IP ":%d failed",
                 CONFIG_EPPP_SRV_TCP_PROXY_TARGET_PORT);
        close(client_fd);
        close(backend_fd);
        vTaskDelete(NULL);
        return;
    }

    char *buf = malloc(PROXY_BUF_SIZE);
    if (!buf) {
        close(client_fd);
        close(backend_fd);
        vTaskDelete(NULL);
        return;
    }

    int maxfd = (client_fd > backend_fd ? client_fd : backend_fd) + 1;

    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(client_fd, &fds);
        FD_SET(backend_fd, &fds);

        struct timeval tv = { .tv_sec = 30, .tv_usec = 0 };
        int ret = select(maxfd, &fds, NULL, NULL, &tv);
        if (ret <= 0)
            break;

        if (FD_ISSET(client_fd, &fds)) {
            int n = recv(client_fd, buf, PROXY_BUF_SIZE, 0);
            if (n <= 0) break;
            if (send(backend_fd, buf, n, 0) <= 0) break;
        }
        if (FD_ISSET(backend_fd, &fds)) {
            int n = recv(backend_fd, buf, PROXY_BUF_SIZE, 0);
            if (n <= 0) break;
            if (send(client_fd, buf, n, 0) <= 0) break;
        }
    }

    free(buf);
    close(client_fd);
    close(backend_fd);
    vTaskDelete(NULL);
}

static void tcp_proxy_task(void *arg)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        ESP_LOGE(TAG, "proxy: listen socket() failed");
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port        = htons(CONFIG_EPPP_SRV_TCP_PROXY_LISTEN_PORT),
    };

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
        listen(listen_fd, 4) < 0) {
        ESP_LOGE(TAG, "proxy: bind/listen on port %d failed",
                 CONFIG_EPPP_SRV_TCP_PROXY_LISTEN_PORT);
        close(listen_fd);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "TCP proxy: :%d -> " CONFIG_EPPP_SRV_TCP_PROXY_TARGET_IP ":%d",
             CONFIG_EPPP_SRV_TCP_PROXY_LISTEN_PORT,
             CONFIG_EPPP_SRV_TCP_PROXY_TARGET_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0)
            continue;

        proxy_conn_t *conn = malloc(sizeof(proxy_conn_t));
        if (!conn) {
            close(client_fd);
            continue;
        }
        conn->client_fd = client_fd;

        if (xTaskCreate(proxy_forward_task, "proxy_fwd", 4096, conn, 5, NULL) != pdPASS) {
            ESP_LOGW(TAG, "proxy: failed to create forward task");
            free(conn);
            close(client_fd);
        }
    }
}

#endif /* CONFIG_EPPP_SRV_TCP_PROXY_ENABLE */

void app_main(void)
{
    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /* Note: esp_netif_init() and WiFi init are handled internally by wifi_manager */

#if CONFIG_EPPP_SRV_LED_GPIO >= 0
    /* Configure status LED (low-active): start with LED off (HIGH) */
    gpio_config_t led_conf = {
        .pin_bit_mask = 1ULL << CONFIG_EPPP_SRV_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led_conf);
    gpio_set_level(CONFIG_EPPP_SRV_LED_GPIO, 1);
#endif

    /* Start WiFi manager — connects from NVS credentials or opens captive portal */
    esp_bus_init();
    wifi_manager_init(&(wifi_manager_config_t){
        .enable_captive_portal = true,
        .stop_ap_on_connect    = true,
        .http = {
            .enable        = true,
            .api_base_path = "/api/wifi",
        },
    });
    ESP_LOGI(TAG, "WiFi manager started");

    /* Start console REPL for WiFi CLI commands */
    init_console();

    /* Wait until WiFi is up before starting EPPP */
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    wifi_manager_wait_connected(0);
    ESP_LOGI(TAG, "WiFi connected, starting EPPP SPI slave...");

#if CONFIG_EPPP_SRV_TCP_PROXY_ENABLE
    /* Stop the wifi_manager HTTP server so port 80 is free for the TCP proxy */
    httpd_handle_t wm_httpd = wifi_manager_get_httpd();
    if (wm_httpd) {
        httpd_stop(wm_httpd);
        ESP_LOGI(TAG, "WiFi manager HTTP server stopped, handing port %d to TCP proxy",
                 CONFIG_EPPP_SRV_TCP_PROXY_LISTEN_PORT);
    }
#endif

    /* Start EPPP server (SPI slave), non-blocking */
    eppp_config_t config = EPPP_DEFAULT_SERVER_CONFIG();
    config.transport = EPPP_TRANSPORT_SPI;
    config.spi.is_master = false;
    config.spi.host = SPI2_HOST;
    config.spi.mosi = CONFIG_SPI_EPPP_PIN_MOSI;
    config.spi.miso = CONFIG_SPI_EPPP_PIN_MISO;
    config.spi.sclk = CONFIG_SPI_EPPP_PIN_SCLK;
    config.spi.cs   = CONFIG_SPI_EPPP_PIN_CS;
    config.spi.intr = CONFIG_SPI_EPPP_PIN_INT;
    config.task.run_task = false;

    ESP_LOGI(TAG, "Starting EPPP SPI slave...");
    esp_netif_t *eppp_netif = eppp_init(EPPP_SERVER, &config);
    if (eppp_netif == NULL)
    {
        ESP_LOGE(TAG, "EPPP init failed");
        return;
    }
    eppp_netif_start(eppp_netif);
    xTaskCreate(eppp_perform_task, "eppp", 4096, eppp_netif, 5, NULL);

    /* Enable NAT so host can reach the internet */
    esp_err_t nat_err = esp_netif_napt_enable(eppp_netif);
    if (nat_err != ESP_OK)
        ESP_LOGW(TAG, "NAPT enable failed: %s", esp_err_to_name(nat_err));
    else
        ESP_LOGI(TAG, "EPPP SPI slave started, NAT enabled");

    /* Start periodic status task */
    xTaskCreate(eppp_server_status_task, "eppp_srv_status", 3072, eppp_netif, 1, NULL);

#if CONFIG_EPPP_SRV_TCP_PROXY_ENABLE
    xTaskCreate(tcp_proxy_task, "tcp_proxy", 4096, NULL, 5, NULL);
#endif
}
