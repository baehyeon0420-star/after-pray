/**
 * SSD1306 라이브러리 설정 파일.
 * 프로젝트 설정: STM32F411 (NUCLEO-F411RE), I2C1, 128x64, 주소 0x3C
 */

#ifndef __SSD1306_CONF_H__
#define __SSD1306_CONF_H__

// MCU 계열
#define STM32F4

// 통신 방식
#define SSD1306_USE_I2C

// I2C 설정 (확정 소자: 디바이스마트 DIS060009, SSD1306 계열)
#define SSD1306_I2C_PORT        hi2c1
#define SSD1306_I2C_ADDR        (0x3C << 1)

// 사용할 폰트만 포함 (나머지는 플래시 절약을 위해 제외)
#define SSD1306_INCLUDE_FONT_6x8
#define SSD1306_INCLUDE_FONT_7x10
#define SSD1306_INCLUDE_FONT_11x18

// 화면 크기 (기본값이 128x64라 별도 지정 불필요)
// #define SSD1306_WIDTH        128
// #define SSD1306_HEIGHT       64

#endif /* __SSD1306_CONF_H__ */
