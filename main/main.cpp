#include "ClassScansionBLTE.hpp"
#include "EspLcdGFX.hpp"
#include "Eyes.hpp"
#include "library.hpp"

static const char *TAG = "Consumer";

#define LCD_HOST SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)
#define LCD_BK_LIGHT_ON_LEVEL 1

#define PIN_NUM_SCLK (gpio_num_t)38
#define PIN_NUM_MOSI (gpio_num_t)39
#define PIN_NUM_LCD_DC (gpio_num_t)45
#define PIN_NUM_LCD_RST (gpio_num_t)40
#define PIN_NUM_LCD_CS (gpio_num_t)21
#define PIN_NUM_BK_LIGHT (gpio_num_t)46

#define LCD_H_RES 320
#define LCD_V_RES 172

esp_lcd_panel_handle_t panel_handle = NULL;
EspLcdGFX *gfx = nullptr;
RoboEyes<EspLcdGFX> *robotEyes = nullptr;

void lcd_init() {
  ESP_LOGI("LCD", "Turn off LCD backlight");
  gpio_config_t bk_gpio_config = {
      .pin_bit_mask = 1ULL << (int)PIN_NUM_BK_LIGHT,
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&bk_gpio_config);
  gpio_set_level((gpio_num_t)PIN_NUM_BK_LIGHT, !LCD_BK_LIGHT_ON_LEVEL);

  ESP_LOGI("LCD", "Initialize SPI bus");
  spi_bus_config_t buscfg = {};
  buscfg.sclk_io_num = PIN_NUM_SCLK;
  buscfg.mosi_io_num = PIN_NUM_MOSI;
  buscfg.miso_io_num = -1;
  buscfg.quadwp_io_num = -1;
  buscfg.quadhd_io_num = -1;
  buscfg.max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t) + 8;

  ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

  ESP_LOGI("LCD", "Install panel IO");
  esp_lcd_panel_io_handle_t io_handle = NULL;
  esp_lcd_panel_io_spi_config_t io_config = {};
  io_config.dc_gpio_num = PIN_NUM_LCD_DC;
  io_config.cs_gpio_num = PIN_NUM_LCD_CS;
  io_config.pclk_hz = LCD_PIXEL_CLOCK_HZ;
  io_config.lcd_cmd_bits = 8;
  io_config.lcd_param_bits = 8;
  io_config.spi_mode = 0;
  io_config.trans_queue_depth = 10;
  io_config.on_color_trans_done = NULL;

  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,
                                           &io_config, &io_handle));

  ESP_LOGI("LCD", "Install ST7789 panel driver");
  esp_lcd_panel_dev_config_t panel_config = {};
  panel_config.reset_gpio_num = PIN_NUM_LCD_RST;
  panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
  panel_config.bits_per_pixel = 16;

  ESP_ERROR_CHECK(
      esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

  // WaveShare ST7789 usually requires color inversion
  esp_lcd_panel_invert_color(panel_handle, true);
  esp_lcd_panel_set_gap(panel_handle, 0, 34); // Gap on Y axis when in landscape
  esp_lcd_panel_swap_xy(panel_handle, true);
  esp_lcd_panel_mirror(panel_handle, false,
                       true); // adjust orientation if upside down

  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

  ESP_LOGI("LCD", "Turn on LCD backlight");
  gpio_set_level((gpio_num_t)PIN_NUM_BK_LIGHT, LCD_BK_LIGHT_ON_LEVEL);
}

static void task_scan_ble(void *pvParameters) {
  ScansionBLE *radar = (ScansionBLE *)pvParameters;
  // Wait for robotEyes to be initialized
  while (robotEyes == nullptr) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  while (true) {
    infoScanning scanRes;
    scanRes = radar->consumeInfo();
    
    uint32_t now = millis();
    uint32_t elapsed = now - scanRes.last_seen_ms;

    // Se abbiamo visto un telefono negli ultimi 5 secondi
    if (scanRes.nbr_of_device > 0 && elapsed < 5000) {
      if (scanRes.ptenza_rsi < -70) {
        ESP_LOGI(TAG, "RSSI: %d, lontana", scanRes.ptenza_rsi);
        robotEyes->setMood(DEFAULT);
        robotEyes->setDisplayColors(0x0000, 0x001F); // Blue
      } else if (scanRes.ptenza_rsi < -50) {
        ESP_LOGI(TAG, "RSSI: %d, vicina", scanRes.ptenza_rsi);
        robotEyes->setMood(HAPPY);
        robotEyes->setDisplayColors(0x0000, 0x07E0); // Green
      } else {
        ESP_LOGI(TAG, "RSSI: %d, molto vicina", scanRes.ptenza_rsi);
        robotEyes->setMood(ANGRY);
        robotEyes->setDisplayColors(0x0000, 0xF800); // Red
        robotEyes->anim_confused();
      }
    } else {
      // Nessun telefono rilevato di recente
      robotEyes->setMood(DEFAULT);
      robotEyes->setDisplayColors(0x0000, 0xFFFF); // White
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

  ESP_LOGI("main", "Initializing LCD...");
  lcd_init();

  // Instantiate GFX and RoboEyes
  gfx = new EspLcdGFX(panel_handle, LCD_H_RES, LCD_V_RES);
  robotEyes = new RoboEyes<EspLcdGFX>(*gfx);
  robotEyes->begin(LCD_H_RES, LCD_V_RES, 30); // 30 fps
  
  // Make eyes bigger (Screen is 320x172)
  robotEyes->setWidth(110, 110);
  robotEyes->setHeight(110, 110);
  robotEyes->setBorderradius(20, 20);
  robotEyes->setSpacebetween(35);
  
  robotEyes->setIdleMode(true, 2, 2);
  robotEyes->setAutoblinker(true, 3, 2);

  ESP_LOGI("main", "Calling logicCore()...");

  ScansionBLE radar;
  radar.init();
  xTaskCreatePinnedToCore(task_scan_ble, "task_scan_ble", 4096, &radar, 1, NULL,
                          1);

  while (true) {
    robotEyes->update();
    vTaskDelay(pdMS_TO_TICKS(33)); // ~30 fps update loop
  }
}