#include "soc.h"

void SOC_Init(SOC_Module *soc, float initial_soc_percent, float capacity_mAh)
{
    soc->capacity_mAh   = capacity_mAh;
    soc->soc_percent    = initial_soc_percent;
    soc->current_offset = 0.0f;
}

void SOC_CalibrateOffset(SOC_Module *soc, float offset_A)
{
    soc->current_offset = offset_A;
}

float SOC_Update(SOC_Module *soc, float current_A, float dt_s)
{
    /* 1. Zero-Current Offset 보정 적용 */
    float current_corrected = current_A - soc->current_offset;

    /* 2. 전하량 적산: A -> mA, s -> h 로 단위 변환 후 dQ(mAh) 계산
     *    SOC(k+1) = SOC(k) + I*dt / Q  를 mAh 기준으로 구현한 것 */
    float dq_mAh = (current_corrected * 1000.0f) * (dt_s / 3600.0f);

    soc->soc_percent += (dq_mAh / soc->capacity_mAh) * 100.0f;

    /* 3. SOC Range 제한 (노션 스펙 명시 요구사항) */
    if (soc->soc_percent > 100.0f) { soc->soc_percent = 100.0f; }
    if (soc->soc_percent < 0.0f)   { soc->soc_percent = 0.0f;   }

    return soc->soc_percent;
}
