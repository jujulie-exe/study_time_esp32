#pragma once

#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"

// Basic Adafruit-style GFX wrapper for esp_lcd
class EspLcdGFX {
private:
    esp_lcd_panel_handle_t panel_handle;
    int16_t width, height;
    uint16_t *framebuffer;

    // Helper to swap bytes for ST7789 (Expects Big-Endian)
    inline uint16_t swapColor(uint16_t color) {
        return (color << 8) | (color >> 8);
    }

    // Helper functions for drawing
    inline void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
        fillRect(x, y, 1, h, color);
    }
    
    inline void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
        fillRect(x, y, w, 1, color);
    }

    void fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t corners, int16_t delta, uint16_t color) {
        int16_t f = 1 - r;
        int16_t ddF_x = 1;
        int16_t ddF_y = -2 * r;
        int16_t x = 0;
        int16_t y = r;

        while (x < y) {
            if (f >= 0) {
                y--;
                ddF_y += 2;
                f += ddF_y;
            }
            x++;
            ddF_x += 2;
            f += ddF_x;

            if (corners & 0x1) {
                drawFastVLine(x0 + x, y0 - y, 2 * y + 1 + delta, color);
                drawFastVLine(x0 + y, y0 - x, 2 * x + 1 + delta, color);
            }
            if (corners & 0x2) {
                drawFastVLine(x0 - x, y0 - y, 2 * y + 1 + delta, color);
                drawFastVLine(x0 - y, y0 - x, 2 * x + 1 + delta, color);
            }
        }
    }

public:
    EspLcdGFX(esp_lcd_panel_handle_t handle, int16_t w, int16_t h) : panel_handle(handle), width(w), height(h) {
        // Try allocating in RAM, if it fails, fallback to standard malloc (or PSRAM if configured)
        // A 320x172 RGB565 framebuffer is 110,080 bytes.
        framebuffer = (uint16_t *)heap_caps_malloc(width * height * sizeof(uint16_t), MALLOC_CAP_DMA);
        if(!framebuffer) {
            framebuffer = (uint16_t *)malloc(width * height * sizeof(uint16_t));
        }
        clearDisplay();
    }

    ~EspLcdGFX() {
        if(framebuffer) {
            free(framebuffer);
        }
    }

    void drawPixel(int16_t x, int16_t y, uint16_t color) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        framebuffer[y * width + x] = swapColor(color);
    }

    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
        if (x >= width || y >= height || w <= 0 || h <= 0) return;
        if (x < 0) { w += x; x = 0; }
        if (y < 0) { h += y; y = 0; }
        if (x + w > width) w = width - x;
        if (y + h > height) h = height - y;

        uint16_t swapped = swapColor(color);
        for (int16_t row = 0; row < h; row++) {
            uint16_t *line = &framebuffer[(y + row) * width + x];
            for (int16_t col = 0; col < w; col++) {
                line[col] = swapped;
            }
        }
    }

    void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
        int16_t max_radius = ((w < h) ? w : h) / 2;
        if (r > max_radius) r = max_radius;
        fillRect(x + r, y, w - 2 * r, h, color);
        // draw four corners
        fillCircleHelper(x + w - r - 1, y + r, r, 1, h - 2 * r - 1, color);
        fillCircleHelper(x + r, y + r, r, 2, h - 2 * r - 1, color);
    }

    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
        int16_t steep = abs(y1 - y0) > abs(x1 - x0);
        if (steep) {
            int16_t t = x0; x0 = y0; y0 = t;
            t = x1; x1 = y1; y1 = t;
        }
        if (x0 > x1) {
            int16_t t = x0; x0 = x1; x1 = t;
            t = y0; y0 = y1; y1 = t;
        }
        int16_t dx, dy;
        dx = x1 - x0;
        dy = abs(y1 - y0);

        int16_t err = dx / 2;
        int16_t ystep;

        if (y0 < y1) ystep = 1;
        else ystep = -1;

        for (; x0 <= x1; x0++) {
            if (steep) drawPixel(y0, x0, color);
            else drawPixel(x0, y0, color);
            err -= dy;
            if (err < 0) {
                y0 += ystep;
                err += dx;
            }
        }
    }

    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
        int16_t a, b, y, last;

        if (y0 > y1) { int16_t t=y0; y0=y1; y1=t; t=x0; x0=x1; x1=t; }
        if (y1 > y2) { int16_t t=y1; y1=y2; y2=t; t=x1; x1=x2; x2=t; }
        if (y0 > y1) { int16_t t=y0; y0=y1; y1=t; t=x0; x0=x1; x1=t; }

        if(y0 == y2) {
            a = b = x0;
            if(x1 < a) a = x1; else if(x1 > b) b = x1;
            if(x2 < a) a = x2; else if(x2 > b) b = x2;
            drawFastHLine(a, y0, b-a+1, color);
            return;
        }

        int16_t dx01 = x1 - x0, dy01 = y1 - y0,
                dx02 = x2 - x0, dy02 = y2 - y0,
                dx12 = x2 - x1, dy12 = y2 - y1;
        int32_t sa = 0, sb = 0;

        if(y1 == y2) last = y1; 
        else last = y1-1; 

        for(y=y0; y<=last; y++) {
            a = x0 + sa / dy01;
            b = x0 + sb / dy02;
            sa += dx01;
            sb += dx02;
            if(a > b) { int16_t t=a; a=b; b=t; }
            drawFastHLine(a, y, b-a+1, color);
        }

        sa = (int32_t)dx12 * (y - y1);
        sb = (int32_t)dx02 * (y - y0);
        for(; y<=y2; y++) {
            a = x1 + sa / dy12;
            b = x0 + sb / dy02;
            sa += dx12;
            sb += dx02;
            if(a > b) { int16_t t=a; a=b; b=t; }
            drawFastHLine(a, y, b-a+1, color);
        }
    }

    void clearDisplay() {
        if(framebuffer) {
            memset(framebuffer, 0, width * height * sizeof(uint16_t));
        }
    }

    void display() {
        if(panel_handle && framebuffer) {
            esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, width, height, framebuffer);
        }
    }
};
