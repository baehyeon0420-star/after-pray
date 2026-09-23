/*
 * bms_io.h
 * MCU <-> 회로(Protection/Charger) 사이의 디지털 신호. 회로팀과 약속한 목록과 1:1.
 *
 *   출력 (MCU -> 회로)          입력 (회로 -> MCU)
 *   ----------------------     ----------------------------
 *   CHG_EN     충전 허용        CHRG       충전 중   (TP5000, LOW=활성)
 *   LOAD_EN    방전 허용        STDBY      충전 완료 (TP5000, LOW=활성)
 *   DCIR_PULSE DCIR 전류 스텝   INA_ALERT  INA228 경보 (LOW=활성)
 *
 * 규약: 출력은 1=허용/켬, 0=차단/끔. PCB에 풀다운이 있어 MCU 리셋 시 기본 차단.
 * 핀 번호는 회로팀 답이 오면 bms_io.c 위쪽 표에서만 바꾼다.
 */
#ifndef BMS_IO_H
#define BMS_IO_H

#include <stdint.h>

void    bms_io_init(void);                    /* 모든 출력을 차단 상태로 */
void    bms_io_set_charge_enable(uint8_t on); /* CHG_EN */
void    bms_io_set_load_enable(uint8_t on);   /* LOAD_EN */
void    bms_io_set_dcir_pulse(uint8_t on);    /* DCIR_PULSE */
void    bms_io_all_off(void);                 /* Fault 시: 충전·방전·DCIR 모두 차단 */

uint8_t bms_io_charger_charging(void);        /* CHRG   : 충전 중이면 1 */
uint8_t bms_io_charger_done(void);            /* STDBY  : 충전 완료면 1 */
uint8_t bms_io_ina_alert(void);               /* INA_ALERT 활성이면 1 */

#endif /* BMS_IO_H */
