#include "ClassScansionBLTE.hpp"

int logicCore() {
  ScansionBLE radar;
  radar.init();
  infoScanning scanRes;
  scanRes = radar.consumeInfo();
  if (scanRes.nbr_of_device > 0) {
    if (scanRes.ptenza_rsi > -70) {
      ESP_LOGI("logicCore", "Person detected");
    }
  }
  return 0;
}