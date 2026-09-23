/*
 * bms_display.h
 * OLED(SSD1306 128x64) 화면 모듈. 흐름도의 "OLED 표시" 상자.
 *
 * - bms_display_init()  : 시작 시 한 번. OLED가 I2C에 응답하면 1, 없으면 0 반환.
 *                         없으면 이후 update는 아무것도 하지 않음 (UART 출력은 그대로 동작).
 * - bms_display_update(): 0.5초마다 호출. 전압/전류/온도/SOC/상태를 그림.
 */
#ifndef BMS_DISPLAY_H
#define BMS_DISPLAY_H

#include <stdint.h>

uint8_t bms_display_init(void);

void bms_display_update(float V, float I_A, float T_C,
                        uint8_t soc_valid, float soc_percent,
                        const char *state_name);

#endif /* BMS_DISPLAY_H */
