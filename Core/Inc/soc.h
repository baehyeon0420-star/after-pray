#ifndef SOC_H
#define SOC_H

#include <stdint.h>

/*
 * SOC_Module
 * ----------
 * 노션 스펙 기준 필요한 4가지 입력을 구조체로 묶어둠:
 *   - Current            : SOC_Update() 인자로 매번 전달
 *   - Sampling Period(dt): SOC_Update() 인자로 매번 전달
 *   - Battery Capacity   : capacity_mAh
 *   - Initial SOC        : soc_percent (SOC_Init에서 100.0f로 설정)
 */
typedef struct {
    float capacity_mAh;    // 배터리 정격 용량 (mAh), 예: 3000.0f
    float soc_percent;     // 현재 SOC (0.0 ~ 100.0 %)
    float current_offset;  // Zero-Current Offset 보정값 (A)
} SOC_Module;

/* 초기화: 배터리 용량과 초기 SOC(MVP 기준 100%) 설정 */
void SOC_Init(SOC_Module *soc, float initial_soc_percent, float capacity_mAh);

/* Zero-Current 상태에서 반복 측정한 평균값을 오프셋으로 저장 */
void SOC_CalibrateOffset(SOC_Module *soc, float offset_A);

/*
 * 매 샘플링 주기(dt_s)마다 호출.
 * current_A: 충전 +, 방전 - (INA228 실측값 또는 더미값)
 * dt_s     : 샘플링 주기(초), 10Hz면 0.1f
 * 반환값   : 갱신된 SOC(%)
 */
float SOC_Update(SOC_Module *soc, float current_A, float dt_s);

#endif /* SOC_H */
