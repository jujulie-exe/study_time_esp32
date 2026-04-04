#include "ClassScansionBLTE.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "nvs_flash.h"
static void task_scan_ble(void *pvParameters) {
  ScansionBLE *radar = (ScansionBLE *)pvParameters;
  while (true) {
    infoScanning scanRes;
    scanRes = radar->consumeInfo();
    if (scanRes.nbr_of_device > 0) {
      if (scanRes.ptenza_rsi > -70) {
        ESP_LOGI("logicCore", "Person detected");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// Forward declaration
extern "C" {
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
}

extern "C" void app_main(void) {
  ESP_LOGI("main", "Initializing NVS (required for Bluetooth)...");
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_LOGI("main", "Calling logicCore()...");

  // Chiamiamo la tua logica (una sola volta, dato che fa init e return 0)

  // Loop infinito per impedire che il task main termini
  ScansionBLE radar;
  radar.init();
  xTaskCreatePinnedToCore(task_scan_ble, "task_scan_ble", 4096, &radar, 1, NULL,
                          1);
}