#ifndef CLASSSCANSIONBLTE_HPP
#define CLASSSCANSIONBLTE_HPP
#define ESP_BLE_AD_TYPE_MANU 0xFF
#include "library.hpp"
struct infoScanning {
  uint8_t nbr_of_device;
  int8_t ptenza_rsi;
};
enum class CompanyID : uint16_t {
  APPLE = 0x004C,
  GOOGLE = 0x0006,
  Samsung = 0x007D,
  Xiaomi = 0x007D,
  Huawei = 0x007D,
  Oppo = 0x007D,
  Vivo = 0x007D,
  Realme = 0x007D,
  OnePlus = 0x007D,
};
struct ManufacturerEntry {
  CompanyID company_id;
  const char *brand;
};

class ScansionBLE {
public:
  ScansionBLE();
  ScansionBLE(ScansionBLE &&) = delete;
  ScansionBLE(const ScansionBLE &) = delete;
  ScansionBLE &operator=(ScansionBLE &&) = delete;
  ScansionBLE &operator=(const ScansionBLE &) = delete;

  int8_t init();
  static void callback(esp_gap_ble_cb_event_t event,
                       esp_ble_gap_cb_param_t *param);
  void writeResEvent(esp_ble_gap_cb_param_t *param);
  bool isPhone(esp_ble_gap_cb_param_t *param) const;
  infoScanning consumeInfo();
  ~ScansionBLE();

private:
  std::atomic<infoScanning> _sharedData{infoScanning{0, -100}};
  static ScansionBLE *_instance;
  esp_ble_scan_params_t _scan_params;
};

#endif // CLASSSCANSIONBLTE_HPP
