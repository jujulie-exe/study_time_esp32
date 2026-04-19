#include "ClassScansionBLTE.hpp"
#include "esp_gap_ble_api.h"
#include "esp_timer.h"
static const ManufacturerEntry PHONE_MANUFACTURERS[] = {
    {CompanyID::APPLE, "Apple"},     {CompanyID::GOOGLE, "Google"},
    {CompanyID::Samsung, "Samsung"}, {CompanyID::Xiaomi, "Xiaomi"},
    {CompanyID::Huawei, "Huawei"},   {CompanyID::Oppo, "Oppo"},
    {CompanyID::Vivo, "Vivo"},       {CompanyID::Realme, "Realme"},
    {CompanyID::OnePlus, "OnePlus"},
};
ScansionBLE *ScansionBLE::_instance = nullptr;
ScansionBLE::ScansionBLE() {}
ScansionBLE::~ScansionBLE() {}

static const char *TAG = "ScansionBLE";

int8_t ScansionBLE::init() {
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

  esp_err_t ret = esp_bt_controller_init(&bt_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "controller init failed: %s", esp_err_to_name(ret));
    return -1;
  }

  ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "controller enable failed: %s", esp_err_to_name(ret));
    return -2;
  }

  ret = esp_bluedroid_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "bluedroid init failed: %s", esp_err_to_name(ret));
    return -3;
  }

  ret = esp_bluedroid_enable();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "bluedroid enable failed: %s", esp_err_to_name(ret));
    return -4;
  }

  _instance = this;

  ret = esp_ble_gap_register_callback(callback);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "gap register failed: %s", esp_err_to_name(ret));
    return -5;
  }

  esp_ble_ext_scan_params_t ext_scan_params = {};

  ext_scan_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
  ext_scan_params.filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
  ext_scan_params.scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE;
  ext_scan_params.cfg_mask = ESP_BLE_GAP_EXT_SCAN_CFG_UNCODE_MASK; // PHY 1M

  ext_scan_params.uncoded_cfg.scan_type = BLE_SCAN_TYPE_ACTIVE;
  ext_scan_params.uncoded_cfg.scan_interval = 0x500; // 800ms
  ext_scan_params.uncoded_cfg.scan_window = 0x30;    // 30ms

  ret = esp_ble_gap_set_ext_scan_params(&ext_scan_params);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "set ext scan params failed: %s", esp_err_to_name(ret));
    return -6;
  }

  return 0;
}
static bool checkCompanyID(uint16_t company_id) {
  for (int i = 0;
       i < sizeof(PHONE_MANUFACTURERS) / sizeof(PHONE_MANUFACTURERS[0]); i++) {
    if (PHONE_MANUFACTURERS[i].company_id ==
        static_cast<CompanyID>(company_id)) {
      ESP_LOGI(TAG, "rilevato telefono: %s", PHONE_MANUFACTURERS[i].brand);
      return true;
    }
  }
  ESP_LOGI(TAG, "altro dispositivo: %d", company_id);
  return false;
}

static uint8_t *resolveExtAdvData(uint8_t *data, uint16_t data_len,
                                  uint8_t type, uint8_t *out_len) {
  // IA generate parser
  if (!data || data_len == 0)
    return nullptr;

  uint16_t i = 0;
  while (i < data_len) {
    uint8_t len = data[i]; // lunghezza del AD (type + payload)
    if (len == 0)
      break;
    if (i + len >= data_len)
      break;

    uint8_t ad_type = data[i + 1];
    if (ad_type == type) {
      *out_len = len - 1;  // lunghezza solo del payload
      return &data[i + 2]; // punta al payload
    }
    i += len + 1; // avanza al prossimo AD
  }
  *out_len = 0;
  return nullptr;
}
bool ScansionBLE::isPhone(esp_ble_gap_cb_param_t *param) const {
  uint8_t *adv_data = param->ext_adv_report.params.adv_data;
  uint8_t adv_data_len = param->ext_adv_report.params.adv_data_len;
  uint8_t out_len = 0;
  uint8_t manu_len = 0;
  uint8_t *manu_ptr = resolveExtAdvData(adv_data, adv_data_len,
                                        ESP_BLE_AD_TYPE_MANU, &manu_len);
  uint8_t *app_ptr = resolveExtAdvData(adv_data, adv_data_len,
                                       ESP_BLE_AD_TYPE_APPEARANCE, &out_len);

  if (app_ptr && out_len >= 2) {
    uint16_t appearance = app_ptr[0] | (app_ptr[1] << 8);
    if (appearance == 0x0040) {
      ESP_LOGI(TAG, "rilevato telefono tramite apparance");
      return true;
    }
  } else if (app_ptr == NULL) {
    if (manu_ptr && manu_len >= 2) {
      uint16_t company_id = manu_ptr[0] | (manu_ptr[1] << 8);
      if (checkCompanyID(company_id)) {
        ESP_LOGI(TAG, "rilevato telefono");
        return true;
      }
    }
  }
  return false;
}

void ScansionBLE::writeResEvent(esp_ble_gap_cb_param_t *param) {
  if (isPhone(param)) {
    infoScanning temp;
    temp.ptenza_rsi = param->ext_adv_report.params.rssi;
    temp.nbr_of_device = 1; 
    temp.last_seen_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    _sharedData.store(temp, std::memory_order_release);
  } else {
    ESP_LOGD(TAG, "nessun telefono");
  }
}

void ScansionBLE::callback(esp_gap_ble_cb_event_t event,
                           esp_ble_gap_cb_param_t *param) {
  switch (event) {

  case ESP_GAP_BLE_SET_EXT_SCAN_PARAMS_COMPLETE_EVT:
    if (param->set_ext_scan_params.status == ESP_BT_STATUS_SUCCESS) {
      esp_ble_gap_start_ext_scan(0, 0);
    }
    break;

  case ESP_GAP_BLE_EXT_SCAN_START_COMPLETE_EVT:
    if (param->ext_scan_start.status != ESP_BT_STATUS_SUCCESS) {
      ESP_LOGE("ScansionBLE", "ext scan start failed");
    }
    break;

  case ESP_GAP_BLE_EXT_ADV_REPORT_EVT:
    if (_instance) {
      _instance->writeResEvent(param);
    }
    break;

  default:
    break;
  }
}
infoScanning ScansionBLE::consumeInfo() {
  return _sharedData.load(std::memory_order_acquire);
}