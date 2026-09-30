/*
 * oled_test.h
 * OLED(SSD1306) 연결 점검 전용 모듈. BMS 로직과 무관하게 따로 쓸 수 있다.
 *
 * 사용법 (main.c)
 *   #include "oled_test.h"
 *   oled_test_run();        // 전원 켤 때 한 번, 또는 키 입력으로 호출
 *
 * 출력은 전부 printf(UART)로 나간다.
 */
#ifndef OLED_TEST_H
#define OLED_TEST_H

#include <stdint.h>

void    oled_test_pins(void);      /* 1단계: SCL/SDA 선 상태 확인 */
void    oled_test_scan(void);      /* 2단계: I2C 주소 스캔 */
uint8_t oled_test_display(void);   /* 3단계: OLED 초기화 + 시험 화면. 성공이면 1 */
void    oled_test_run(void);       /* 1~3단계를 순서대로 실행 */

#endif /* OLED_TEST_H */
