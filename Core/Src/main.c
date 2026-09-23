/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "soc.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
  BMS_IDLE = 0,
  BMS_CHARGING,
  BMS_DISCHARGING
} bms_state_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define INA228_ADDR 0x40
#define REG_VBUS    0x05
#define I_IDLE_THRESHOLD_A  0.05f  // 전류 크기가 50 mA 이하면 IDLE (임시값)
#define V_FULL_THRESHOLD_V   4.15f  // 만충 판정 전압 (임시값)
#define I_FULL_TERMINATE_A   0.10f  // 충전 종료 전류 (임시값, TP5000 확인 필요)
#define V_EMPTY_THRESHOLD_V  2.50f  // 저전압 임계 (임시값, BQ29700 확인 필요)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
volatile uint8_t rx_buf = 0;       // HAL이 채우는 자리
volatile uint8_t rx_cmd = 0;       // 처리할 글자
volatile uint8_t rx_new = 0;       // 새 글자 도착 표시
uint8_t manual_mode = 0;           // 1이면 키 입력으로 전압·전류 지정
float   manual_I_A = 0.0f;
float   manual_V_V = 3.70f;
SOC_Module bms_soc;                // SOC 계산 모듈 (soc.c)
volatile uint8_t tick_100ms = 0;
uint32_t tick_count = 0;
bms_state_t bms_state = BMS_IDLE;  // 현재 배터리 상태
uint8_t soc_valid = 0;             // 0이면 SOC 미정 (흐름도의 "SOC 유효?")
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2) {
    tick_100ms = 1;
  }
}

/* 더미 전류: 60초 시나리오 (흐름도 규약: 방전 +, 충전 -) */
static float dummy_current_A(uint32_t tick)
{
  uint32_t t = (tick / 10) % 60;
  if (t < 15) return 0.84f;              // 0~14초 방전
  if (t < 30) return 0.0f;               // 15~29초 대기
  if (t < 45) return -1.5f;              // 30~44초 충전
  return 0.0f;                           // 45~59초 충전 끝난 뒤 대기
}

/* 더미 전압: 위 시나리오에 맞춰 방전 끝과 충전 끝을 만들어 줌 */
static float dummy_voltage_V(uint32_t tick)
{
  uint32_t t = (tick / 10) % 60;
  if (t < 15) return 3.70f - (t * 0.09f);        // 3.70 V -> 2.44 V (저전압 도달)
  if (t < 30) return 3.00f;                      // 대기
  if (t < 45) return 3.00f + ((t - 30) * 0.08f); // 3.00 V -> 4.12 V (아직 만충 아님)
  return 4.18f;                                  // 전류 0에서 만충 조건 성립
}

/* S: 상태 판정. 전류 크기가 임계값 이하면 IDLE */
static bms_state_t bms_decide_state(float I_A)
{
  if (I_A >  I_IDLE_THRESHOLD_A) return BMS_DISCHARGING;
  if (I_A < -I_IDLE_THRESHOLD_A) return BMS_CHARGING;
  return BMS_IDLE;
}

static const char *bms_state_name(bms_state_t s)
{
  switch (s) {
    case BMS_DISCHARGING: return "DISCHARGING";
    case BMS_CHARGING:    return "CHARGING";
    default:              return "IDLE";
  }
}

/* J: 재동기. 확실히 아는 순간에 SOC를 강제로 맞추고 유효 표시 */
static void bms_resync(float V, float I_A)
{
  float I_abs = (I_A < 0.0f) ? -I_A : I_A;

  if (V >= V_FULL_THRESHOLD_V && I_abs <= I_FULL_TERMINATE_A) {
    bms_soc.soc_percent = 100.0f;      // 만충
    soc_valid = 1;
  } else if (bms_state == BMS_DISCHARGING && V <= V_EMPTY_THRESHOLD_V) {
    bms_soc.soc_percent = 0.0f;        // 완전 방전
    soc_valid = 1;
  }
}
/* UART로 한 글자 받을 때마다 자동 호출 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2) {
    rx_cmd = rx_buf;
    rx_new = 1;
    HAL_UART_Receive_IT(&huart2, (uint8_t *)&rx_buf, 1);   // 다음 글자 준비
  }
}

/* 키 입력 처리. 수동 모드에서는 시나리오 대신 지정한 값을 사용 */
static void bms_handle_command(uint8_t c)
{
  switch (c) {
    case 'd': manual_mode = 1; manual_I_A =  0.84f; break;
    case 'c': manual_mode = 1; manual_I_A = -1.50f; break;
    case 'i': manual_mode = 1; manual_I_A =  0.00f; break;
    case '+': manual_mode = 1; manual_V_V += 0.10f;
              if (manual_V_V > 4.30f) manual_V_V = 4.30f; break;
    case '-': manual_mode = 1; manual_V_V -= 0.10f;
              if (manual_V_V < 2.00f) manual_V_V = 2.00f; break;
    case 'a': manual_mode = 0; break;
    case 'r': soc_valid = 0; bms_soc.soc_percent = 0.0f; break;
    case 'h': printf("[명령] d=방전 c=충전 i=대기 +/-=전압조절 a=자동 r=SOC미정\r\n"); return;
    default:  return;
  }
  printf("[입력 %c] 모드=%s V=%dmV I=%dmA\r\n", c, manual_mode ? "수동" : "자동",
         (int)(manual_V_V * 1000.0f), (int)(manual_I_A * 1000.0f));
}
static void bms_task_100ms(void)
{
  tick_count++;
  /* PC에서 키를 눌렀으면 처리 */
    if (rx_new) { rx_new = 0; bms_handle_command(rx_cmd); }

  /* G: 전압, 전류 읽기 (소자 오기 전까지 더미값) */
    float I_A = manual_mode ? manual_I_A : dummy_current_A(tick_count);
    float V   = manual_mode ? manual_V_V : dummy_voltage_V(tick_count);

  /* S: 상태 판정 */
  bms_state = bms_decide_state(I_A);

  /* VC + H~I2: SOC가 유효할 때만 적산하고 계산 */
  if (soc_valid) {
    SOC_Update(&bms_soc, -I_A, 0.1f);
  }

  /* J: 재동기 */
  bms_resync(V, I_A);

  /* M: 5회마다(0.5초) 출력. 나중에 OLED로 교체 */
  if (tick_count % 5 == 0) {
    int v_mv = (int)(V * 1000.0f + 0.5f);
    if (soc_valid) {
      int soc_x100 = (int)(bms_soc.soc_percent * 100.0f + 0.5f);
      printf("t=%lus V=%dmV I=%dmA %s SOC: %d.%02d %%\r\n",
             tick_count / 10, v_mv, (int)(I_A * 1000.0f),
             bms_state_name(bms_state), soc_x100 / 100, soc_x100 % 100);
    } else {
      printf("t=%lus V=%dmV I=%dmA %s SOC: --\r\n",
             tick_count / 10, v_mv, (int)(I_A * 1000.0f),
             bms_state_name(bms_state));
    }
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  HAL_UART_Receive_IT(&huart2, (uint8_t *)&rx_buf, 1);   // 키 입력 받기 시작
    printf("\r\n[명령] d=방전 c=충전 i=대기 +/-=전압조절 a=자동 r=SOC미정 h=도움말\r\n");
  SOC_Init(&bms_soc, 100.0f, 3000.0f);   // 시작 SOC 100%, 용량 3000 mAh (30Q)
  SOC_CalibrateOffset(&bms_soc, 0.0f);   // Zero-Current Offset, 실측 전까지 0
  HAL_TIM_Base_Start_IT(&htim2);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  if (tick_100ms) {
	       tick_100ms = 0;
	       bms_task_100ms();
	     }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 8400-1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 1000-1;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
