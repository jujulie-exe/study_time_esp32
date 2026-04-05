#include "ClassScansionBLTE.hpp"
#include "library.hpp"

static const char *TAG = "Consumer";
static void task_scan_ble(void *pvParameters) {
  ScansionBLE *radar = (ScansionBLE *)pvParameters;
  while (true) {
    infoScanning scanRes;
    scanRes = radar->consumeInfo();
    if (scanRes.nbr_of_device > 0) {
      if (scanRes.ptenza_rsi < -70)
        ESP_LOGI(TAG, "RSSI: %d, lontana", scanRes.ptenza_rsi);
      else if (scanRes.ptenza_rsi < -50)
        ESP_LOGI(TAG, "RSSI: %d, vicina", scanRes.ptenza_rsi);
      else
        ESP_LOGI(TAG, "RSSI: %d, molto vicina", scanRes.ptenza_rsi);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
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

  ScansionBLE radar;
  radar.init();
  xTaskCreatePinnedToCore(task_scan_ble, "task_scan_ble", 4096, &radar, 1, NULL,
                          1);

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}