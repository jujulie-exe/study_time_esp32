#include "ClassScansionBLTE.hpp"

ScansionBLE *ScansionBLE::_instance = nullptr;

ScansionBLE::ScansionBLE() {}

ScansionBLE::~ScansionBLE() {}
static void initParmScan(esp_ble_scan_params_t &scan_params) {
  scan_params.scan_type = BLE_SCAN_TYPE_PASSIVE;
  scan_params.scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE;
  scan_params.scan_interval = 0x500;
  scan_params.scan_window = 0x30;
  return;
}
int8_t ScansionBLE::init() {
  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
  ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
  ESP_ERROR_CHECK(esp_bluedroid_init());
  ESP_ERROR_CHECK(esp_bluedroid_enable());
  _instance = this;
  esp_ble_gap_register_callback(callback);
  initParmScan(_scan_params);
  esp_ble_gap_set_scan_params(&_scan_params);
  return 0;
}

void ScansionBLE::wirteResEvent(esp_ble_gap_cb_param_t *param) {
  infoScanning temp = _sharedData.load();
  temp.ptenza_rsi = param->scan_rst.rssi;
  temp.nbr_of_device++;
  _sharedData.store(temp, std::memory_order_release);
}

void ScansionBLE::callback(esp_gap_ble_cb_event_t event,
                           esp_ble_gap_cb_param_t *param) {
  if (event == ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT) {
    uint32_t duration = 0;
    esp_ble_gap_start_scanning(duration);
  }
  if (event == ESP_GAP_BLE_SCAN_RESULT_EVT) {
    if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
      _instance->wirteResEvent(param);
    }
  }
  return;
}

infoScanning ScansionBLE::consumeInfo() {
  infoScanning scanRes;
  scanRes = _sharedData.load();
  _sharedData.store(infoScanning{0, -100}, std::memory_order_release);
  return scanRes;
}