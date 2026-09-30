/*
 * oled_test.c
 * OLED(SSD1306, I2C) 연결 점검 전용. 배선 문제를 단계별로 좁히기 위한 코드.
 *
 * 전제: I2C1 = PB8(SCL), PB9(SDA), OLED 주소 0x3C (모듈 표기 0x78)
 *       핀이 바뀌면 아래 OLED_TEST_* 정의만 고치면 된다.
 */
#include "oled_test.h"
#include "main.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <stdio.h>

extern I2C_HandleTypeDef hi2c1;

#define OLED_TEST_PORT     GPIOB
#define OLED_TEST_SCL_PIN  GPIO_PIN_8
#define OLED_TEST_SDA_PIN  GPIO_PIN_9
#define OLED_TEST_AF       GPIO_AF4_I2C1
#define OLED_TEST_ADDR7    0x3C

/* I2C 핀을 일반 입력으로 바꿔 선 상태를 읽고, 다시 I2C로 되돌린다 */
static void pins_as_input(uint32_t pull)
{
  GPIO_InitTypeDef g = {0};
  g.Pin   = OLED_TEST_SCL_PIN | OLED_TEST_SDA_PIN;
  g.Mode  = GPIO_MODE_INPUT;
  g.Pull  = pull;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(OLED_TEST_PORT, &g);
  HAL_Delay(2);
}

static void pins_as_i2c(uint32_t pull)
{
  GPIO_InitTypeDef g = {0};
  g.Pin       = OLED_TEST_SCL_PIN | OLED_TEST_SDA_PIN;
  g.Mode      = GPIO_MODE_AF_OD;
  g.Pull      = pull;
  g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  g.Alternate = OLED_TEST_AF;
  HAL_GPIO_Init(OLED_TEST_PORT, &g);
}

void oled_test_pins(void)
{
  printf("\r\n[1단계] 선 상태 확인 (SCL=PB8, SDA=PB9)\r\n");

  HAL_I2C_DeInit(&hi2c1);

  pins_as_input(GPIO_NOPULL);
  int scl_a = HAL_GPIO_ReadPin(OLED_TEST_PORT, OLED_TEST_SCL_PIN);
  int sda_a = HAL_GPIO_ReadPin(OLED_TEST_PORT, OLED_TEST_SDA_PIN);

  pins_as_input(GPIO_PULLDOWN);   /* 내부 풀다운으로 눌러도 High면 외부 풀업이 있다 */
  int scl_b = HAL_GPIO_ReadPin(OLED_TEST_PORT, OLED_TEST_SCL_PIN);
  int sda_b = HAL_GPIO_ReadPin(OLED_TEST_PORT, OLED_TEST_SDA_PIN);

  printf("  그냥 읽기   : SCL=%d SDA=%d\r\n", scl_a, sda_a);
  printf("  눌러서 읽기 : SCL=%d SDA=%d\r\n", scl_b, sda_b);

  if (scl_b == 1 && sda_b == 1) {
    printf("  판정: 정상. 모듈 전원과 풀업이 살아 있음\r\n");
  } else if (scl_b == 0 && sda_b == 0) {
    printf("  판정: 두 선 다 안 붙음. VCC/GND가 모듈에 제대로 안 들어감\r\n");
  } else {
    printf("  판정: %s 쪽만 연결됨. 나머지 한 선 확인 필요\r\n", (scl_b == 1) ? "SCL" : "SDA");
  }

  /* I2C 원복 */
  pins_as_i2c(GPIO_NOPULL);
  HAL_I2C_Init(&hi2c1);
}

void oled_test_scan(void)
{
  uint8_t found = 0;
  printf("\r\n[2단계] I2C 주소 스캔 (0x03~0x77)\r\n");
  for (uint8_t a = 0x03; a <= 0x77; a++) {
    if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(a << 1), 2, 5) == HAL_OK) {
      printf("  응답: 0x%02X%s\r\n", a, (a == OLED_TEST_ADDR7) ? "  <- OLED" : "");
      found++;
    }
  }
  if (found == 0) {
    printf("  응답 없음. 1단계 판정을 먼저 해결할 것\r\n");
  } else {
    printf("  총 %d개 (OLED=0x3C/0x3D, INA228=0x40)\r\n", found);
  }
}

uint8_t oled_test_display(void)
{
  printf("\r\n[3단계] 화면 출력\r\n");

  if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(OLED_TEST_ADDR7 << 1), 3, 20) != HAL_OK) {
    printf("  0x%02X 응답 없음. 화면 출력 생략\r\n", OLED_TEST_ADDR7);
    return 0;
  }

  ssd1306_Init();

  /* 전체 켜기 -> 전체 끄기: 패널이 살아 있으면 눈에 보인다 */
  ssd1306_Fill(White);
  ssd1306_UpdateScreen();
  HAL_Delay(500);
  ssd1306_Fill(Black);
  ssd1306_UpdateScreen();
  HAL_Delay(200);

  /* 글자와 테두리 */
  ssd1306_SetCursor(2, 2);
  ssd1306_WriteString("Hello BMS!", Font_7x10, White);
  ssd1306_SetCursor(2, 16);
  ssd1306_WriteString("OLED TEST OK", Font_7x10, White);
  ssd1306_DrawRectangle(0, 30, 127, 45, White);
  ssd1306_FillRectangle(2, 32, 80, 43, White);
  ssd1306_SetCursor(2, 50);
  ssd1306_WriteString("I2C 0x3C 128x64", Font_6x8, White);
  ssd1306_UpdateScreen();

  printf("  성공. 흰 화면이 한 번 번쩍인 뒤 글자가 보이면 정상\r\n");
  return 1;
}

void oled_test_run(void)
{
  printf("\r\n===== OLED 점검 시작 =====\r\n");
  oled_test_pins();
  oled_test_scan();
  oled_test_display();
  printf("===== OLED 점검 끝 =====\r\n");
}
