/*
 * bms_sensors_dummy.c
 * 소자가 없을 때 쓰는 가짜 센서. 60초 시나리오를 반복하거나, 키 입력값을 그대로 돌려준다.
 * 실제 INA228/NTC 코드가 준비되면 이 파일을 빌드에서 빼고 그 파일을 넣으면 된다.
 */
#include "bms_sensors.h"
#include "main.h"     /* HAL_GetTick */

static uint8_t manual_mode = 0;
static float   manual_I_A  = 0.0f;
static float   manual_V_V  = 3.70f;

/* 60초 시나리오 (방전 15초 -> 대기 15초 -> 충전 15초 -> 충전 완료 대기 15초) */
static uint32_t scenario_sec(void)
{
  return (HAL_GetTick() / 1000U) % 60U;
}

static float scenario_current_A(uint32_t t)
{
  if (t < 15) return 0.84f;    /* 방전 */
  if (t < 30) return 0.0f;     /* 대기 */
  if (t < 45) return -1.5f;    /* 충전 */
  return 0.0f;                 /* 충전 끝난 뒤 대기 (만충 조건) */
}

static float scenario_voltage_V(uint32_t t)
{
  if (t < 15) return 3.70f - (t * 0.09f);         /* 3.70 -> 2.44 V (저전압 도달) */
  if (t < 30) return 3.00f;
  if (t < 45) return 3.00f + ((t - 30) * 0.08f);  /* 3.00 -> 4.12 V (아직 만충 아님) */
  return 4.18f;                                   /* 전류 0에서 만충 */
}

uint8_t bms_sensors_init(void)              { return 0; }
uint8_t bms_sensors_ok(void)                { return 1; }
float   bms_sensors_read_temp_C(uint8_t ch) { (void)ch; return 25.0f; }

float bms_sensors_read_current_A(void)
{
  return manual_mode ? manual_I_A : scenario_current_A(scenario_sec());
}

float bms_sensors_read_voltage_V(void)
{
  return manual_mode ? manual_V_V : scenario_voltage_V(scenario_sec());
}

void bms_sensors_dummy_manual(uint8_t on)      { manual_mode = on ? 1 : 0; }
void bms_sensors_dummy_set_current(float I_A)  { manual_mode = 1; manual_I_A = I_A; }
uint8_t bms_sensors_dummy_is_manual(void)      { return manual_mode; }

void bms_sensors_dummy_adjust_voltage(float dV)
{
  manual_mode = 1;
  manual_V_V += dV;
  if (manual_V_V > 4.30f) manual_V_V = 4.30f;
  if (manual_V_V < 2.00f) manual_V_V = 2.00f;
}
