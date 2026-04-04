#ifndef CLASSSCANSIONBLTE_HPP
#define CLASSSCANSIONBLTE_HPP
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"
#include <atomic>

struct infoScanning {
  uint8_t nbr_of_device;
  int8_t ptenza_rsi;
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
  void wirteResEvent(esp_ble_gap_cb_param_t *param);
  infoScanning consumeInfo();
  ~ScansionBLE();

private:
  std::atomic<infoScanning> _sharedData{{0, -100}};
  static ScansionBLE *_instance;
  esp_ble_scan_params_t _scan_params;
};

#endif // CLASSSCANSIONBLTE_HPP
