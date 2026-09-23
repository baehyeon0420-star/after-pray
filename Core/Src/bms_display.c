/*
 * bms_display.c
 * OLED(SSD1306 128x64, I2C 0x3C) 화면 그리기.
 *
 * 화면 배치 (7x10 폰트, 한 줄 18글자, 줄 간격 12픽셀)
 *   y= 0 : V 3.700V  T 25.3C
 *   y=12 : I  +840mA
 *   y=24 : SOC  87.65 %
 *   y=38 : [########      ]   <- SOC 막대
 *   y=52 : DISCHARGING
 */
#include "bms_display.h"
#include "main.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <stdio.h>

static uint8_t oled_present = 0;

uint8_t bms_display_init(void)
{
  /* OLED가 실제로 꽂혀 있는지 먼저 확인.
   * 라이브러리는 I2C 쓰기를 무한 대기로 하므로, 없는 상태에서 Init을 부르면 멈출 수 있음. */
  if (HAL_I2C_IsDeviceReady(&SSD1306_I2C_PORT, SSD1306_I2C_ADDR, 2, 20) != HAL_OK) {
    oled_present = 0;
    return 0;
  }

  ssd1306_Init();
  ssd1306_Fill(Black);
  ssd1306_SetCursor(0, 0);
  ssd1306_WriteString("Hello BMS!", Font_7x10, White);   /* S2 성공 기준 */
  ssd1306_UpdateScreen();

  oled_present = 1;
  return 1;
}

void bms_display_update(float V, float I_A, float T_C,
                        uint8_t soc_valid, float soc_percent,
                        const char *state_name)
{
  if (!oled_present) return;

  char line[32];
  int v_mv  = (int)(V * 1000.0f + 0.5f);
  int i_ma  = (int)(I_A * 1000.0f + (I_A >= 0 ? 0.5f : -0.5f));
  int t_x10 = (int)(T_C * 10.0f + 0.5f);

  ssd1306_Fill(Black);

  /* 1줄: 전압, 온도 */
  snprintf(line, sizeof line, "V %d.%03dV  T %d.%dC",
           v_mv / 1000, v_mv % 1000, t_x10 / 10, t_x10 % 10);
  ssd1306_SetCursor(0, 0);
  ssd1306_WriteString(line, Font_7x10, White);

  /* 2줄: 전류 (부호 표시) */
  snprintf(line, sizeof line, "I %c%dmA", (i_ma < 0) ? '-' : '+', (i_ma < 0) ? -i_ma : i_ma);
  ssd1306_SetCursor(0, 12);
  ssd1306_WriteString(line, Font_7x10, White);

  /* 3줄: SOC 숫자 */
  if (soc_valid) {
    int s_x100 = (int)(soc_percent * 100.0f + 0.5f);
    snprintf(line, sizeof line, "SOC %3d.%02d %%", s_x100 / 100, s_x100 % 100);
  } else {
    snprintf(line, sizeof line, "SOC   --");
  }
  ssd1306_SetCursor(0, 24);
  ssd1306_WriteString(line, Font_7x10, White);

  /* 4줄: SOC 막대. 테두리 0~127, 안쪽을 SOC 비율만큼 채움 */
  ssd1306_DrawRectangle(0, 38, 127, 46, White);
  if (soc_valid) {
    uint8_t w = (uint8_t)((soc_percent / 100.0f) * 124.0f);
    if (w > 0) ssd1306_FillRectangle(2, 40, 2 + w, 44, White);
  }

  /* 5줄: 상태 */
  ssd1306_SetCursor(0, 52);
  ssd1306_WriteString((char *)state_name, Font_7x10, White);

  ssd1306_UpdateScreen();
}
