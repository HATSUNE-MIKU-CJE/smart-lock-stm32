/**
  ******************************************************************************
  * @file    Key.c
  * @brief   4×3 矩阵键盘驱动（状态机消抖 + 事件缓冲）
  *          行列扫描方式读取按键，支持数字键0~9、确认键(*)、返回键(#)
  *          行线：PA0~PA3（推挽输出，默认高电平）
  *          列线：PA4~PA6（上拉输入）
  *          按键布局：
  *                     C0   C1   C2
  *                R0:   1    2    3
  *                R1:   4    5    6
  *                R2:   7    8    9
  *                R3:   *    0    #
  ******************************************************************************
  */

#include "stm32f10x.h"
#include "Key.h"
#include "Delay.h"

/*================== 私有类型定义 ==================*/

typedef enum {
    KEY_STATE_IDLE,              // 空闲：按键未被操作
    KEY_STATE_PRESS_DEBOUNCE,    // 按下消抖：等待20ms确认不是抖动
    KEY_STATE_PRESSED,           // 已确认按下：此时可上报按键事件
    KEY_STATE_RELEASE_DEBOUNCE   // 释放消抖：等待20ms确认不是抖动
} KeyState_t;

/**
  * @brief 按键运行时状态结构体
  * @note  每个按键独立维护自己的消抖状态、上次电平和时间戳
  */
typedef struct {
    KeyState_t state;         // 当前处于消抖状态机的哪个阶段
    uint8_t    last_level;    // 上次扫描时的电平（1=未按，0=按下）
    uint32_t   debounce_time; // 进入消抖状态时的系统时间戳（单位：ms）
} KeyRuntime_t;

/*================== 私有常量 ==================*/

/*----- 行线定义（4根，推挽输出） -----*/
static GPIO_TypeDef* const row_ports[4] = {GPIOA, GPIOA, GPIOA, GPIOA};
static const uint16_t row_pins[4] = {GPIO_Pin_0, GPIO_Pin_1, GPIO_Pin_2, GPIO_Pin_3};

/*----- 列线定义（3根，上拉输入） -----*/
static GPIO_TypeDef* const col_ports[3] = {GPIOA, GPIOA, GPIOA};
static const uint16_t col_pins[3] = {GPIO_Pin_4, GPIO_Pin_5, GPIO_Pin_6};

/*----- 按键索引到事件码的映射表 -----*/
/*
 * 矩阵索引与物理按键对应关系：
 *   idx 0: R0,C0 → 1       idx 1: R0,C1 → 2       idx 2: R0,C2 → 3
 *   idx 3: R1,C0 → 4       idx 4: R1,C1 → 5       idx 5: R1,C2 → 6
 *   idx 6: R2,C0 → 7       idx 7: R2,C1 → 8       idx 8: R2,C2 → 9
 *   idx 9: R3,C0 → * → KEY_CONFIRM
 *   idx 10:R3,C1 → 0
 *   idx 11:R3,C2 → # → KEY_BACK
 */
static const uint8_t key_code_map[12] = {
    1, 2, 3,
    4, 5, 6,
    7, 8, 9,
    KEY_CONFIRM, 0, KEY_BACK
};

/*================== 私有变量 ==================*/

static KeyRuntime_t key_runtime[12];    /**< 12个按键的运行时状态 */
static uint8_t pending_event = KEY_NONE; /**< 按键事件缓冲 */

/*================== 私有辅助函数 ==================*/

/**
  * @brief  矩阵键盘硬件扫描
  * @retval 0~11：当前被按下的按键索引
  * @retval -1：没有按键被按下
  * @note   逐行输出低电平，读取列线电平。行列交叉点即为按下的键。
  *         扫描前将所有行置高，当前扫描行置低，延时10us等电平稳定。
  */
static int8_t Matrix_ScanRaw(void)
{
    for (uint8_t r = 0; r < 4; r++)
    {
        /* 所有行先置高电平 */
        for (uint8_t i = 0; i < 4; i++)
        {
            GPIO_SetBits(row_ports[i], row_pins[i]);
        }
        
        /* 当前扫描行置低电平 */
        GPIO_ResetBits(row_ports[r], row_pins[r]);
        
        /* 短暂延时等电平稳定（10us足够覆盖GPIO翻转+按键导通延时） */
        Delay_us(10);
        
        /* 读取3根列线 */
        for (uint8_t c = 0; c < 3; c++)
        {
            if (GPIO_ReadInputDataBit(col_ports[c], col_pins[c]) == 0)
            {
                return (int8_t)(r * 3 + c);  /* 返回按键索引 */
            }
        }
    }
    
    return -1;  /* 没有按键按下 */
}

/**
  * @brief  检测单个按键是否被确认按下（状态机消抖核心）
  * @param  index         按键在映射表中的索引（0~11）
  * @param  current_level 当前扫描得到的电平（0=按下，1=未按）
  * @retval 0：没有新事件
  * @retval 1：该按键被确认按下，可以上报事件
  */
static uint8_t DetectSingleKey(uint8_t index, uint8_t current_level)
{
    KeyRuntime_t* rt = &key_runtime[index];
    uint32_t now = GetTick();
    
    switch (rt->state)
    {
        /*------------------ 空闲 ------------------*/
        case KEY_STATE_IDLE:
            if (current_level == 0 && rt->last_level == 1)
            {
                rt->state = KEY_STATE_PRESS_DEBOUNCE;
                rt->debounce_time = now;
            }
            break;
        
        /*------------------ 按下消抖 ------------------*/
        case KEY_STATE_PRESS_DEBOUNCE:
            if (now - rt->debounce_time >= 20)
            {
                if (current_level == 0)
                {
                    rt->state = KEY_STATE_PRESSED;
                    rt->last_level = 0;
                    return 1;  /* 确认按下，上报事件 */
                }
                else
                {
                    rt->state = KEY_STATE_IDLE;  /* 抖动，回空闲 */
                }
            }
            break;
        
        /*------------------ 已确认按下 ------------------*/
        case KEY_STATE_PRESSED:
            if (current_level == 1 && rt->last_level == 0)
            {
                rt->state = KEY_STATE_RELEASE_DEBOUNCE;
                rt->debounce_time = now;
            }
            break;
        
        /*------------------ 释放消抖 ------------------*/
        case KEY_STATE_RELEASE_DEBOUNCE:
            if (now - rt->debounce_time >= 20)
            {
                rt->state = KEY_STATE_IDLE;
            }
            break;
    }
    
    rt->last_level = current_level;
    return 0;
}

/*================== 对外接口 ==================*/

/**
  * @brief  按键模块初始化
  * @note   1. 开启GPIOA时钟
  *         2. 行线配置为推挽输出（默认高电平）
  *         3. 列线配置为上拉输入
  *         4. 初始化12个按键的运行时状态
  */
void Key_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    GPIO_InitTypeDef GPIO_InitStructure;
    
    /* 行线：推挽输出，默认高电平 */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    for (uint8_t i = 0; i < 4; i++)
    {
        GPIO_InitStructure.GPIO_Pin = row_pins[i];
        GPIO_Init(row_ports[i], &GPIO_InitStructure);
        GPIO_SetBits(row_ports[i], row_pins[i]);
    }
    
    /* 列线：上拉输入 */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    for (uint8_t i = 0; i < 3; i++)
    {
        GPIO_InitStructure.GPIO_Pin = col_pins[i];
        GPIO_Init(col_ports[i], &GPIO_InitStructure);
    }
    
    /* 初始化12个按键的运行时状态 */
    for (uint8_t i = 0; i < 12; i++)
    {
        key_runtime[i].state = KEY_STATE_IDLE;
        key_runtime[i].last_level = 1;      /* 默认高电平（未按） */
        key_runtime[i].debounce_time = 0;
    }
}

/**
  * @brief  扫描所有按键，将检测到的事件存入缓冲
  * @note   主循环中每轮调用。若pending_event未被取走，本轮跳过。
  *         执行流程：矩阵扫描 → 得到当前按下索引 → 逐键状态机消抖 → 上报事件
  */
void Key_Scan(void)
{
    /* 上次事件未取走，不扫描新按键（防止事件覆盖） */
    if (pending_event != KEY_NONE) return;
    
    int8_t current_raw = Matrix_ScanRaw();  /* -1=无按键，0~11=有按键 */
    
    /* 遍历12个按键，逐个更新消抖状态机 */
    for (uint8_t i = 0; i < 12; i++)
    {
        /* 如果矩阵扫描结果等于当前索引，说明该键物理上被按下了（电平=0） */
        uint8_t level = (i == current_raw) ? 0 : 1;
        
        if (DetectSingleKey(i, level))
        {
            pending_event = key_code_map[i];
            break;  /* 一次只处理一个按键 */
        }
    }
}

/**
  * @brief  获取按键事件
  * @retval 按键事件编码：0~9（数字）、KEY_CONFIRM（*键）、KEY_BACK（#键）、KEY_NONE（无事件）
  * @note   取出后pending_event自动清空。
  */
uint8_t Key_GetEvent(void)
{
    uint8_t evt = pending_event;
    pending_event = KEY_NONE;
    return evt;
}
