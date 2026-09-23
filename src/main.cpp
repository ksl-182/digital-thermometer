/*
 * 火灾温度监测 — Proteus + LM35
 * LM35 VOUT -> PA0
 * 数码管：段选 PB0-PB7，位选 PB8-PB11（共阴极，低电平选通）
 *
 * 显示 xx.x，右对齐，消隐防残影
 */

#include <Arduino.h>
#include "stm32f1xx_hal.h"

#define RCC_APB2ENR_  (*(volatile uint32_t *)(0x40021018UL))
#define AFIO_MAPR_    (*(volatile uint32_t *)(0x40010004UL))
#define GPIOB_CRL_    (*(volatile uint32_t *)(0x40010C00UL))
#define GPIOB_CRH_    (*(volatile uint32_t *)(0x40010C04UL))
#define GPIOB_BSRR_   (*(volatile uint32_t *)(0x40010C10UL))

#define LM35_PIN  PA0
#define GREEN_LED PA3
#define RED_LED   PA4
#define TEMP_ALARM  35.0f

static const uint8_t FONT[12] = {
  0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F,
  0x40, // -
  0x00  // 空白
};
#define CHAR_MINUS 10
#define CHAR_BLANK 11

uint8_t dispBuf[4] = {CHAR_BLANK, CHAR_BLANK, CHAR_BLANK, CHAR_BLANK};

// ---------- 数码管 ----------
static void display_hw_init(void) {
  RCC_APB2ENR_ |= (1U << 0) | (1U << 3);
  AFIO_MAPR_ = (AFIO_MAPR_ & ~(7U << 24)) | (2U << 24); // 关 JTAG

  GPIOB_CRL_ = 0x33333333UL;
  GPIOB_CRH_ = (GPIOB_CRH_ & 0xFFFF0000UL) | 0x00003333UL;
  GPIOB_BSRR_ = (0xFFUL << 16) | (0x0FUL << 8);
}

// 空白位不扫描，避免一号位残影
static void scan_once(const uint8_t buf[4]) {
  for (int d = 0; d < 4; d++) {
    if (buf[d] == CHAR_BLANK) continue;

    // 1) 全部位选关闭
    GPIOB_BSRR_ = (0x0FUL << 8);
    // 2) 段全部拉低
    GPIOB_BSRR_ = (0xFFUL << 16);
    for (volatile int i = 0; i < 20; i++);

    // 3) 输出本位段码
    uint8_t seg = buf[d];
    GPIOB_BSRR_ = ((uint32_t)seg) | ((uint32_t)(~seg & 0xFF) << 16);
    for (volatile int i = 0; i < 10; i++);

    // 4) 打开本位
    GPIOB_BSRR_ = (1UL << (8 + d)) << 16;
    for (volatile int i = 0; i < 280; i++);

    // 5) 关闭本位
    GPIOB_BSRR_ = (1UL << (8 + d));
  }
  // 收尾：全灭
  GPIOB_BSRR_ = (0x0FUL << 8) | (0xFFUL << 16);
}

// 温度写入缓冲，右对齐 xx.x
static void updateBuffer(float t) {
  bool neg = (t < 0.0f);
  if (neg) t = -t;

  int raw = (int)(t * 10.0f + 0.5f);
  if (raw < 0) raw = 0;
  if (raw > 999) raw = 999; // 99.9

  int t1 = raw / 100;       // 十位
  int u  = (raw / 10) % 10; // 个位
  int d  = raw % 10;        // 十分位

  if (neg) {
    dispBuf[0] = CHAR_MINUS;
    dispBuf[1] = (t1 > 0) ? FONT[t1] : CHAR_BLANK;
    dispBuf[2] = FONT[u] | 0x80;
    dispBuf[3] = FONT[d];
  } else if (t1 > 0) {
    // 10.0 ~ 99.9 ：空白 + 十位 + 个位. + 十分位
    dispBuf[0] = CHAR_BLANK;
    dispBuf[1] = FONT[t1];
    dispBuf[2] = FONT[u] | 0x80;
    dispBuf[3] = FONT[d];
  } else {
    // 0.0 ~ 9.9 ：两位空白
    dispBuf[0] = CHAR_BLANK;
    dispBuf[1] = CHAR_BLANK;
    dispBuf[2] = FONT[u] | 0x80;
    dispBuf[3] = FONT[d];
  }
}

// ---------- ADC（12-bit，长采样，多次平均）----------
static ADC_HandleTypeDef hadc1;

static void adc_init(void) {
  __HAL_RCC_ADC1_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitTypeDef g = {};
  g.Pin  = GPIO_PIN_0;
  g.Mode = GPIO_MODE_ANALOG;
  HAL_GPIO_Init(GPIOA, &g);

  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  HAL_ADC_Init(&hadc1);

  ADC_ChannelConfTypeDef ch = {};
  ch.Channel = ADC_CHANNEL_0;
  ch.Rank = ADC_REGULAR_RANK_1;
  ch.SamplingTime = ADC_SAMPLETIME_239CYCLES_5; // 长采样，Proteus 更稳
  HAL_ADC_ConfigChannel(&hadc1, &ch);

  HAL_ADCEx_Calibration_Start(&hadc1);
}

static uint16_t adc_read_avg(void) {
  uint32_t sum = 0;
  const int N = 16;
  for (int i = 0; i < N; i++) {
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    sum += HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
  }
  return (uint16_t)(sum / N);
}

// LM35: 10mV/°C
// Proteus 本工程 VDDA 实测按 5V 基准（3.3V 会偏低约 1/3）
//   mV = adc * 5000 / 4095
//   °C = mV / 10
#define ADC_VREF_MV  5000.0f

static float read_temp_c(void) {
  uint16_t adc = adc_read_avg();
  float mv = (float)adc * ADC_VREF_MV / 4095.0f;
  return mv / 10.0f;
}

void setup() {
  display_hw_init();
  adc_init();

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  digitalWrite(GREEN_LED, HIGH); // 上电正常：绿灯
  digitalWrite(RED_LED, LOW);

  // 自检 8888（约 0.5s）
  uint8_t all8[4] = {0x7F, 0x7F, 0x7F, 0x7F};
  unsigned long t0 = millis();
  while (millis() - t0 < 500) scan_once(all8);

  updateBuffer(read_temp_c());
}

void loop() {
  static unsigned long lastRead = 0;
  unsigned long now = millis();

  if (now - lastRead >= 150) {
    lastRead = now;
    float t = read_temp_c();
    updateBuffer(t);

    if (t >= TEMP_ALARM) {
      digitalWrite(RED_LED, HIGH);
      digitalWrite(GREEN_LED, LOW);
    } else {
      digitalWrite(RED_LED, LOW);
      digitalWrite(GREEN_LED, HIGH);
    }
  }

  scan_once(dispBuf);
}
