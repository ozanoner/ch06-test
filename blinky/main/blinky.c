
#include "app_bsp.h"
#include "FreeAct.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_https_ota.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_system.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "devops_easy_connect.h"

#define TAG "app"

typedef struct
{
    Active super;
    TimeEvent te;
    bool green_is_on;
} Blinky;

typedef enum
{
    TIMEOUT_SIG = USER_SIG,
    BUTTON_CLICKED_SIG,
} BlinkySignal_t;

static Blinky blinky;
static StackType_t blinky_stack[8192];
static Event *blinky_queue[10];
static StaticTask_t ota_task_buf;
static StackType_t ota_task_stack[8192];

static void Blinky_ctor(Blinky *const me);
static void Blinky_dispatch(Blinky *const me, Event const *const e);
static void handle_button_click();
static void on_connected(void *arg);

static void print_memory_info()
{
    size_t free_dram = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "Free 8-bit accessible DRAM: %u bytes", free_dram);
}

void app_main()
{
    ESP_LOGI(TAG, "Blinky application");
    print_memory_info();

    app_wifi_init(WIFI_SSID, WIFI_PWD, on_connected, NULL);
    app_wifi_connect();

    AppBSP_init();
    AppBSPButton_set_handler(handle_button_click);

    Blinky_ctor(&blinky);
    Active_start(&blinky.super,
                 1, // priority
                 blinky_queue, sizeof(blinky_queue) / sizeof(blinky_queue[0]),
                 blinky_stack, sizeof(blinky_stack),
                 0 // options
    );
}

static void Blinky_ctor(Blinky *const me)
{
    Active_ctor(&me->super, (DispatchHandler)&Blinky_dispatch);
    me->te.type = TYPE_PERIODIC;
    TimeEvent_ctor(&me->te, USER_SIG, &me->super);
    me->green_is_on = false;
}

static void Blinky_dispatch(Blinky *const me, Event const *const e)
{
    ESP_LOGI(TAG, "Blinky_dispatch: sig=%d", e->sig);

    switch (e->sig)
    {
    case INIT_SIG:
        TimeEvent_arm(&me->te, 3000);
        break;

    case TIMEOUT_SIG:
        AppBSP_toggle_red();
        AppBSP_toggle_blue();
        break;

    case BUTTON_CLICKED_SIG:
        AppBSP_toggle_green();
        me->green_is_on = !me->green_is_on;
        ESP_LOGI(TAG, "Green LED: %s", me->green_is_on ? "ON" : "OFF");
        break;

    default:
        break;
    }
}

static void handle_button_click()
{
    ESP_LOGI(TAG, "Button clicked!");

    static Event e = {.sig = BUTTON_CLICKED_SIG};
    Active_post((Active *)&blinky.super, (Event *)&e);
}

static void run_ota_check(void *arg)
{
    ESP_LOGI(TAG, "WiFi connected, running OTA check...");

    esp_http_client_config_t http_config = {
        .url = "https://github.com/ozanoner/embedded-devops-ch04/releases/latest/download/blinky.bin",
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 16384,
        .buffer_size_tx = 16384,
    };
    esp_https_ota_config_t ota_config = {.http_config = &http_config};
    esp_https_ota_handle_t ota_handle;
    esp_app_desc_t new_app_info;

    esp_err_t err = esp_https_ota_begin(&ota_config, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }

    err = esp_https_ota_get_img_desc(ota_handle, &new_app_info);
    if (err != ESP_OK) {
        esp_https_ota_abort(ota_handle);
        ESP_LOGE(TAG, "OTA version check failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }

    if (strcmp(new_app_info.version, esp_app_get_description()->version) == 0) {
        esp_https_ota_abort(ota_handle);
        ESP_LOGI(TAG, "Firmware is already up to date");
        vTaskDelete(NULL);
        return;
    }

    while ((err = esp_https_ota_perform(ota_handle)) == ESP_ERR_HTTPS_OTA_IN_PROGRESS)
        ;
    if (err != ESP_OK) {
        esp_https_ota_abort(ota_handle);
        ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }

    err = esp_https_ota_finish(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA finish failed: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "OTA update complete, restarting");
    esp_restart();
    vTaskDelete(NULL);
}

static void on_connected(void *arg)
{
    TaskHandle_t created = xTaskCreateStatic(
        run_ota_check,
        "ota_check",
        8192,
        NULL,
        5,
        ota_task_stack,
        &ota_task_buf);

    if (created == NULL)
    {
        ESP_LOGE(TAG, "Failed to create OTA check task");
    }
}