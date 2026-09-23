/*
 * bms_sensors.h
 * 센서 읽기 인터페이스. main.c는 이 함수들만 호출하고, 구현은 소팀별 파일로 바꿔 끼운다.
 *
 *   지금        : bms_sensors_dummy.c   (소자 없을 때 가짜값 + 키 입력 수동 조작)
 *   전압/전류팀 : bms_sensors_ina228.c  (INA228, I2C1 0x40)   <- 만들 파일
 *   NTC팀       : (온도 함수만) 같은 이름으로 구현
 *
 * 규약
 *   - 전류 부호: 방전 +, 충전 -  (흐름도 기준)
 *   - 단위: V, A, °C
 *   - 온도 채널: 0~3 (NTC 4개)
 */
#ifndef BMS_SENSORS_H
#define BMS_SENSORS_H

#include <stdint.h>

#define BMS_NTC_CHANNELS   4

uint8_t bms_sensors_init(void);              /* 0이면 성공 */
float   bms_sensors_read_voltage_V(void);    /* 배터리 전압 */
float   bms_sensors_read_current_A(void);    /* 방전 +, 충전 - */
float   bms_sensors_read_temp_C(uint8_t ch); /* NTC 채널 0~3 */
uint8_t bms_sensors_ok(void);                /* 센서 통신 정상이면 1 (S5 Fault에서 사용) */

/* 더미 구현 전용 조작. 실제 센서 구현 파일에서는 빈 함수로 두면 된다. */
void    bms_sensors_dummy_manual(uint8_t on);        /* 1: 키 입력값 사용, 0: 60초 시나리오 */
void    bms_sensors_dummy_set_current(float I_A);
void    bms_sensors_dummy_adjust_voltage(float dV);
uint8_t bms_sensors_dummy_is_manual(void);

#endif /* BMS_SENSORS_H */
