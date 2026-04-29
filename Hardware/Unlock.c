/**
  ******************************************************************************
  * @file    Unlock.c
  * @brief   开锁/关锁自动流程实现
  *          内部维护4阶段状态机，无需外部干预时序
  ******************************************************************************
  */

#include "stm32f10x.h"
#include "Unlock.h"
#include "Motor.h"
#include "Switch.h"
#include "OLED.h"
#include "Buzzer.h"
#include "Delay.h"   /* GetTick() */

/*================== 私有类型与变量 ==================*/

typedef enum {
    PHASE_IDLE = 0,       /**< 空闲：未在执行开锁流程 */
    PHASE_UNLOCKING,      /**< 开锁中：电机转动，等待微动开关到位 */
    PHASE_UNLOCKED,       /**< 已开锁：到位停止，延时保持 */
    PHASE_LOCKING         /**< 关锁中：继续转动，凸轮离开锁舌 */
} UnlockPhase_t;

static UnlockPhase_t s_phase = PHASE_IDLE;  /**< 当前开锁阶段 */
static uint32_t s_tick = 0;                 /**< 阶段内延时计时 */

/*================== 对外接口 ==================*/

/**
  * @brief  初始化开锁模块
  * @note   上电时调用，确保电机停止，状态归零
  */
void Unlock_Init(void)
{
    Motor_Off();
    s_phase = PHASE_IDLE;
    s_tick = 0;
}

/**
  * @brief  启动开锁流程
  * @note   密码验证成功后调用，电机启动，进入开锁中状态
  */
void Unlock_Start(void)
{
    Motor_On();
    s_phase = PHASE_UNLOCKING;
}

/**
  * @brief  查询开锁模块是否忙碌
  * @retval 1：正在执行开锁/保持/关锁流程
  * @retval 0：空闲，可接受新的开锁指令
  */
uint8_t Unlock_IsBusy(void)
{
    return (s_phase != PHASE_IDLE);
}

/**
  * @brief  开锁状态机驱动（需在主循环中周期性调用）
  * @note   根据当前阶段自动推进：
  *           UNLOCKING → 检测微动开关 → UNLOCKED
  *           UNLOCKED  → 5秒延时到   → LOCKING
  *           LOCKING   → 500ms延时到 → IDLE（关锁完成）
  */
void Unlock_Tick(void)
{
    switch (s_phase)
    {
        case PHASE_UNLOCKING:
            if (Switch_IsClosed())
            {
                Motor_Off();              /* 凸轮顶到位，停止保持 */
                s_tick = GetTick();       /* 记录到位时刻 */
                s_phase = PHASE_UNLOCKED;
                
                OLED_Clear();
                OLED_ShowString(0, 0, "OPEN", OLED_8X16);
                OLED_Update();
            }
            break;
        
        case PHASE_UNLOCKED:
            if (GetTick() - s_tick > 5000)  /* 保持开锁5秒 */
            {
                Motor_On();               /* 继续同向旋转 */
                s_tick = GetTick();
                s_phase = PHASE_LOCKING;
            }
            break;
        
        case PHASE_LOCKING:
            if (GetTick() - s_tick > 500)   /* 继续旋转500ms脱开 */
            {
                Motor_Off();              /* 停止，弹簧自动弹回 */
                s_phase = PHASE_IDLE;
                
                OLED_Clear();
                OLED_ShowString(0, 0, "CLOSED", OLED_8X16);
                OLED_Update();
                Buzzer_Lock();
            }
            break;
        
        default:
            /* IDLE 状态无需处理 */
            break;
    }
}
