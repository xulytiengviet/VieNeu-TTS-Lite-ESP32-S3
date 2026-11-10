/* VieNeu Edge Lab: Hybrid ESP32-S3 Wi-Fi HTTP WAV → I2S demonstration.
 * This DOES NOT run the neural VieNeu TTS model on the microcontroller.
 * Apache-2.0 / Long Ngo research demo.
 *
 * Intended for trusted LAN testing, not production or open Internet access.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "driver/i2s_std.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "wav_pcm.h"

#define WAV_BUFFER_CAPACITY (1024U * 1024U)
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAILED_BIT BIT1
#define WIFI_MAX_RETRIES 10
#define AUDIO_FRAMES_PER_BLOCK 512

static const char *TAG = "vieneu_edge";
static EventGroupHandle_t s_wifi_events;
static int s_wifi_retries = 0;

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
    bool overflowed;
} response_buffer_t;

static void wifi_events(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT);
        if (s_wifi_retries < WIFI_MAX_RETRIES) {
            s_wifi_retries++;
            ESP_LOGW(TAG, "Wi-Fi reconnect attempt %d", s_wifi_retries);
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_wifi_events, WIFI_FAILED_BIT);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_wifi_retries = 0;
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Wi-Fi connected with IP");
    }
}

static bool connect_wifi(void) {
    if (strlen(CONFIG_VIENEU_WIFI_SSID) == 0) {
        ESP_LOGE(TAG, "Set Wi-Fi SSID in menuconfig first");
        return false;
    }
    s_wifi_events = xEventGroupCreate();
    if (!s_wifi_events) {
        ESP_LOGE(TAG, "No memory for Wi-Fi EventGroup");
        return false;
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));

    esp_event_handler_instance_t wifi_instance, ip_instance;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_events, NULL, &wifi_instance));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_events, NULL, &ip_instance));

    wifi_config_t config = {0};
    snprintf((char *)config.sta.ssid, sizeof(config.sta.ssid), "%s", CONFIG_VIENEU_WIFI_SSID);
    snprintf((char *)config.sta.password, sizeof(config.sta.password), "%s", CONFIG_VIENEU_WIFI_PASSWORD);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_events,
        WIFI_CONNECTED_BIT | WIFI_FAILED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(30000));
    bool ok = (bits & WIFI_CONNECTED_BIT) != 0;
    if (!ok) ESP_LOGE(TAG, "Wi-Fi failed or timed out");
    return ok;
}

static esp_err_t collect_http(esp_http_client_event_t *event) {
    response_buffer_t *buffer = (response_buffer_t *)event->user_data;
    if (event->event_id == HTTP_EVENT_ON_DATA) {
        if (!buffer || event->data_len < 0 ||
            (size_t)event->data_len > buffer->capacity - buffer->size) {
            if (buffer) buffer->overflowed = true;
            return ESP_FAIL;
        }
        memcpy(buffer->data + buffer->size, event->data, (size_t)event->data_len);
        buffer->size += (size_t)event->data_len;
    }
    return ESP_OK;
}

static bool retrieve_wav(response_buffer_t *buffer) {
    /* Explicitly restrict prototype to HTTP on a private LAN; TLS is NOT configured. */
    if (strncmp(CONFIG_VIENEU_API_URL, "http://", 7) != 0) {
        ESP_LOGE(TAG, "Prototype accepts http:// LAN URLs only; see security warning");
        return false;
    }

    cJSON *request = cJSON_CreateObject();
    if (!request) return false;
    cJSON_AddStringToObject(request, "model", "vieneu-v3-nano");
    cJSON_AddStringToObject(request, "voice", CONFIG_VIENEU_VOICE);
    cJSON_AddStringToObject(request, "input", CONFIG_VIENEU_TEXT);
    cJSON_AddStringToObject(request, "response_format", "wav");
    cJSON_AddNumberToObject(request, "sample_rate", 24000);
    char *body = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    if (!body) return false;

    esp_http_client_config_t config = {
        .url = CONFIG_VIENEU_API_URL,
        .event_handler = collect_http,
        .user_data = buffer,
        .timeout_ms = 180000,
        .buffer_size = 4096,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        cJSON_free(body);
        return false;
    }
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Accept", "audio/wav");
    if (CONFIG_VIENEU_BEARER_TOKEN[0] != '\0') {
        const char *token = CONFIG_VIENEU_BEARER_TOKEN;
        const size_t size = strlen(token) + 8;
        char *auth = malloc(size);
        if (!auth) {
            esp_http_client_cleanup(client);
            cJSON_free(body);
            return false;
        }
        snprintf(auth, size, "Bearer %s", token);
        esp_http_client_set_header(client, "Authorization", auth);
        free(auth);
    }
    esp_http_client_set_post_field(client, body, (int)strlen(body));

    int64_t started = esp_timer_get_time();
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    int64_t elapsed_ms = (esp_timer_get_time() - started) / 1000;
    ESP_LOGI(TAG, "HTTP status=%d err=%s bytes=%u elapsed=%lld ms",
             status, esp_err_to_name(err), (unsigned)buffer->size, (long long)elapsed_ms);
    esp_http_client_cleanup(client);
    cJSON_free(body);
    if (buffer->overflowed) {
        ESP_LOGE(TAG, "WAV exceeds 1 MiB buffer limit; shorten the input");
        return false;
    }
    if (err != ESP_OK || status != 200 || buffer->size == 0) {
        ESP_LOGE(TAG, "Request did not return an audio payload");
        return false;
    }
    return true;
}

static esp_err_t play_pcm_i2s(const wav_pcm_view_t *wav) {
    i2s_chan_handle_t channel = NULL;
    i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    esp_err_t err = i2s_new_channel(&chan, &channel, NULL);
    if (err != ESP_OK) return err;

    i2s_std_config_t format = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(24000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)CONFIG_VIENEU_I2S_BCLK,
            .ws = (gpio_num_t)CONFIG_VIENEU_I2S_WS,
            .dout = (gpio_num_t)CONFIG_VIENEU_I2S_DOUT,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {0},
        },
    };
    err = i2s_channel_init_std_mode(channel, &format);
    if (err == ESP_OK) err = i2s_channel_enable(channel);
    if (err != ESP_OK) {
        i2s_del_channel(channel);
        return err;
    }

    const size_t frames = wav->pcm_bytes / sizeof(int16_t);
    size_t cursor = 0;
    int64_t start = esp_timer_get_time();
    int16_t stereo[AUDIO_FRAMES_PER_BLOCK * 2];

    while (cursor < frames) {
        size_t count = frames - cursor;
        if (count > AUDIO_FRAMES_PER_BLOCK) count = AUDIO_FRAMES_PER_BLOCK;
        for (size_t i = 0; i < count; ++i) {
            const uint8_t *source = wav->pcm + (cursor + i) * 2;
            uint16_t sample = (uint16_t)source[0] | ((uint16_t)source[1] << 8);
            stereo[2 * i] = (int16_t)sample;
            stereo[2 * i + 1] = (int16_t)sample;
        }
        const uint8_t *out = (const uint8_t *)stereo;
        size_t remaining = count * 2 * sizeof(int16_t);
        while (remaining > 0) {
            size_t written = 0;
            err = i2s_channel_write(channel, out, remaining, &written,
                                    pdMS_TO_TICKS(5000));
            if (err != ESP_OK || written == 0) {
                ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(err));
                goto finish;
            }
            out += written;
            remaining -= written;
        }
        cursor += count;
    }

finish:
    ESP_LOGI(TAG, "I2S submitted %u/%u samples in %lld ms",
             (unsigned)cursor, (unsigned)frames,
             (long long)((esp_timer_get_time() - start) / 1000));
    /* A few DMA frames can remain queued after the last write. */
    vTaskDelay(pdMS_TO_TICKS(100));
    i2s_channel_disable(channel);
    i2s_del_channel(channel);
    return err;
}

void app_main(void) {
    ESP_LOGI(TAG, "VieNeu hybrid prototype | NOT offline neural TTS");
    ESP_LOGI(TAG, "Heap before Wi-Fi: %u bytes", (unsigned)esp_get_free_heap_size());
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    if (!connect_wifi()) return;

    response_buffer_t response = {
        .data = heap_caps_malloc(WAV_BUFFER_CAPACITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
        .capacity = WAV_BUFFER_CAPACITY,
    };
    if (!response.data) {
        ESP_LOGE(TAG, "Need at least 1 MiB PSRAM for prototype buffer; configure SPIRAM");
        return;
    }
    ESP_LOGI(TAG, "Heap before HTTP: %u bytes", (unsigned)esp_get_free_heap_size());

    if (retrieve_wav(&response)) {
        wav_pcm_view_t wav;
        const char *problem = wav_pcm_parse(response.data, response.size, &wav);
        if (problem) {
            ESP_LOGE(TAG, "Rejected WAV: %s", problem);
        } else {
            ESP_LOGI(TAG, "PCM mono 16bit rate=%lu frames=%u duration=%.3fs",
                     (unsigned long)wav.sample_rate, (unsigned)(wav.pcm_bytes / 2),
                     ((double)wav.pcm_bytes / 2.0) / wav.sample_rate);
            err = play_pcm_i2s(&wav);
            ESP_LOGI(TAG, "I2S result: %s", esp_err_to_name(err));
        }
    }
    ESP_LOGI(TAG, "Free heap after: %u bytes", (unsigned)esp_get_free_heap_size());
    heap_caps_free(response.data);
    ESP_LOGI(TAG, "Demo finished; reset to run a new request");
}
