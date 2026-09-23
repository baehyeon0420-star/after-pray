/*
 * bms_io.c
 * 회로 제어·상태 신호 구현.
 *
 * 아직 핀이 확정되지 않아 BMS_IO_PINS_ASSIGNED = 0 (상태만 기억하는 스텁).
 * 회로팀 답이 오면:
 *   1) CubeMX에서 아래 핀을 GPIO_Output / GPIO_Input(EXTI)로 잡고 GENERATE CODE
 *   2) 아래 표의 포트/핀을 채우고 BMS_IO_PINS_ASSIGNED 를 1로
 *
 *   신호        제안 핀   Nucleo 위치
 *   CHG_EN      PB10      CN10-25 (D6)
 *   LOAD_EN     PB4       CN10-27 (D5)
 *   DCIR_PULSE  PB5       CN10-29 (D4)
 *   CHRG        PA6       CN10-13 (D12)
 *   STDBY       PA7       CN10-15 (D11)
 *   INA_ALERT   PA8       CN10-23 (D7)
 */
#include "bms_io.h"
#include "main.h"

#define BMS_IO_PINS_ASSIGNED   0

#if BMS_IO_PINS_ASSIGNED
#define CHG_EN_PORT      GPIOB
#define CHG_EN_PIN       GPIO_PIN_10
#define LOAD_EN_PORT     GPIOB
#define LOAD_EN_PIN      GPIO_PIN_4
#define DCIR_PULSE_PORT  GPIOB
#define DCIR_PULSE_PIN   GPIO_PIN_5
#define CHRG_PORT        GPIOA
#define CHRG_PIN         GPIO_PIN_6
#define STDBY_PORT       GPIOA
#define STDBY_PIN        GPIO_PIN_7
#define INA_ALERT_PORT   GPIOA
#define INA_ALERT_PIN    GPIO_PIN_8
#endif

static uint8_t chg_en = 0, load_en = 0, dcir_pulse = 0;

static void write_out(uint8_t which, uint8_t on)
{
#if BMS_IO_PINS_ASSIGNED
  GPIO_TypeDef *port = (which == 0) ? CHG_EN_PORT : (which == 1) ? LOAD_EN_PORT : DCIR_PULSE_PORT;
  uint16_t      pin  = (which == 0) ? CHG_EN_PIN  : (which == 1) ? LOAD_EN_PIN  : DCIR_PULSE_PIN;
  HAL_GPIO_WritePin(port, pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
#else
  (void)which; (void)on;
#endif
}

void bms_io_init(void)                    { bms_io_all_off(); }
void bms_io_set_charge_enable(uint8_t on) { chg_en = on ? 1 : 0;     write_out(0, chg_en); }
void bms_io_set_load_enable(uint8_t on)   { load_en = on ? 1 : 0;    write_out(1, load_en); }
void bms_io_set_dcir_pulse(uint8_t on)    { dcir_pulse = on ? 1 : 0; write_out(2, dcir_pulse); }

void bms_io_all_off(void)
{
  bms_io_set_dcir_pulse(0);
  bms_io_set_load_enable(0);
  bms_io_set_charge_enable(0);
}

/* 입력 신호는 LOW=활성 (오픈드레인 + 풀업) */
uint8_t bms_io_charger_charging(void)
{
#if BMS_IO_PINS_ASSIGNED
  return HAL_GPIO_ReadPin(CHRG_PORT, CHRG_PIN) == GPIO_PIN_RESET;
#else
  return 0;
#endif
}

uint8_t bms_io_charger_done(void)
{
#if BMS_IO_PINS_ASSIGNED
  return HAL_GPIO_ReadPin(STDBY_PORT, STDBY_PIN) == GPIO_PIN_RESET;
#else
  return 0;
#endif
}

uint8_t bms_io_ina_alert(void)
{
#if BMS_IO_PINS_ASSIGNED
  return HAL_GPIO_ReadPin(INA_ALERT_PORT, INA_ALERT_PIN) == GPIO_PIN_RESET;
#else
  return 0;
#endif
}
