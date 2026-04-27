/**
  ******************************************************************************
  * @file    Key.c
  * @brief   12路独立按键驱动（状态机消抖 + 事件缓冲）
  *          支持数字键0~9、确认键、返回键，按键接GND，GPIO配置为上拉输入
  ******************************************************************************
  */

#include "stm32f10x.h"
#include "Key.h"
#include "Delay.h"

/*================== 私有类型定义 ==================*/

/**
  * @brief 按键消抖状态机枚举
  * @note  4个状态构成一个闭环：IDLE → PRESS_DEBOUNCE → PRESSED → RELEASE_DEBOUNCE → IDLE
  */
typedef enum {
    KEY_STATE_IDLE,              // 空闲：按键未被操作
    KEY_STATE_PRESS_DEBOUNCE,    // 按下消抖：检测到下降沿，等待20ms确认不是抖动
    KEY_STATE_PRESSED,           // 已确认按下：此时可上报按键事件
    KEY_STATE_RELEASE_DEBOUNCE   // 释放消抖：检测到上升沿，等待20ms确认不是抖动
} KeyState_t;

/**
  * @brief 按键硬件信息结构体（只读，上电后不变）
  * @note  描述每个按键接在哪个GPIO端口/引脚，以及它的逻辑编号
  */
typedef struct {
    GPIO_TypeDef* port;     // GPIO端口，如GPIOA、GPIOB
    uint16_t pin;           // 引脚号，如GPIO_Pin_0
    uint8_t  key_code;      // 按键逻辑编号：0~9表示数字，KEY_CONFIRM/KEY_BACK表示功能键
} KeyHardware_t;

/**
  * @brief 按键运行时状态结构体（会变化，需要记忆）
  * @note  每个按键独立维护自己的消抖状态、上次电平和时间戳
  */
typedef struct {
    KeyState_t state;         // 当前处于消抖状态机的哪个阶段
    uint8_t    last_level;    // 上次扫描读到的电平（1=未按，0=按下）
    uint32_t   debounce_time; // 进入消抖状态时的系统时间戳（单位：ms）
} KeyRuntime_t;

/*================== 私有常量与变量 ==================*/

/**
  * @brief  12个按键的硬件定义表
  * @note   已修正引脚冲突：避开PA2（舵机PWM）、PB10（蜂鸣器）
  *         若实际杜邦线接法不同，请修改port/pin，但不要改key_code
  */
const KeyHardware_t key_table[12] = {
    {GPIOA, GPIO_Pin_1,  1},            // [0] 数字1
    {GPIOB, GPIO_Pin_11, 2},            // [1] 数字2
    {GPIOA, GPIO_Pin_3,  3},            // [2] 数字3（原PA2冲突，改为PA3）
    {GPIOA, GPIO_Pin_4,  4},            // [3] 数字4
    {GPIOA, GPIO_Pin_5,  5},            // [4] 数字5
    {GPIOA, GPIO_Pin_6,  6},            // [5] 数字6
    {GPIOA, GPIO_Pin_7,  7},            // [6] 数字7
    {GPIOB, GPIO_Pin_0,  8},            // [7] 数字8
    {GPIOB, GPIO_Pin_1,  9},            // [8] 数字9
    {GPIOB, GPIO_Pin_11, 0},            // [9] 数字0
    {GPIOA, GPIO_Pin_0,  KEY_CONFIRM},  // [10] 确认键
    {GPIOB, GPIO_Pin_13, KEY_BACK},     // [11] 返回键（原PB10冲突，改为PB12）
};

/**
  * @brief  12个按键的运行时状态数组
  * @note   与key_table一一对应，key_runtime[i]记录key_table[i]这个按键的实时状态
  */
KeyRuntime_t key_runtime[12];

/**
  * @brief  按键事件缓冲
  * @note   当某按键被确认按下后，其key_code暂存于此，等待主循环取走。
  *         KEY_NONE表示当前没有待处理事件。
  */
static uint8_t pending_event = KEY_NONE;

/*================== 对外接口函数 ==================*/

/**
  * @brief  按键模块初始化
  * @note   1. 开启GPIOA和GPIOB的时钟
  *         2. 将12个按键引脚配置为内部上拉输入（IPU）
  *         3. 初始化每个按键的运行时状态（默认空闲、上次电平为高）
  *         按键接法：一脚接GPIO，另一脚接GND。未按时靠内部上拉保持高电平。
  */
void Key_Init(void)
{
    // 开启GPIOA和GPIOB的APB2时钟，否则后续GPIO_Init无效
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
    
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;   // 内部上拉：按键未按时默认为高电平
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    
    // 遍历12个按键，逐个初始化GPIO和运行时状态
    for (uint8_t i = 0; i < 12; i++) {
        GPIO_InitStructure.GPIO_Pin = key_table[i].pin;
        GPIO_Init(key_table[i].port, &GPIO_InitStructure);
        
        key_runtime[i].state = KEY_STATE_IDLE;      // 初始为空闲状态
        key_runtime[i].last_level = 1;              // IPU默认高电平（未按）
        key_runtime[i].debounce_time = 0;           // 时间戳清零
    }
}

/**
  * @brief  检测单个按键是否被确认按下（状态机消抖核心）
  * @param  index 按键在key_table中的索引（0~11）
  * @retval 0：没有新事件（正在消抖、空闲、或释放中）
  * @retval 1：该按键被确认按下，可以上报事件
  * @note   调用周期：建议在while(1)中每轮都调用，由Key_Scan统一调度
  */
static uint8_t DetectSingleKey(uint8_t index)
{
    // 通过索引查表，获取该按键的硬件信息和运行时状态指针
    GPIO_TypeDef* port = key_table[index].port;
    uint16_t pin = key_table[index].pin;
    KeyRuntime_t* rt = &key_runtime[index];
    
    // 读取当前引脚电平（1=未按，0=按下）
    uint8_t current_level = GPIO_ReadInputDataBit(port, pin);
    // 获取当前系统时间（单位ms，由SysTick中断每1ms累加）
    uint32_t now = GetTick();
    
    // 根据当前状态，进入对应的处理分支
    switch (rt->state) {
        /*------------------ 状态1：空闲 ------------------*/
        case KEY_STATE_IDLE:
            // 检测下降沿：上次是高电平(1)，这次变成低电平(0)，说明手指按下来了
            if (current_level == 0 && rt->last_level == 1) {
                rt->state = KEY_STATE_PRESS_DEBOUNCE;   // 进入按下消抖阶段
                rt->debounce_time = now;                // 记录进入消抖的时刻
            }
            break;
            
        /*------------------ 状态2：按下消抖 ------------------*/
        case KEY_STATE_PRESS_DEBOUNCE:
            // 等待20ms消抖时间。机械按键抖动通常在5~15ms内结束，20ms足够覆盖
            if (now - rt->debounce_time >= 20) {
                // 20ms后再读一次电平，确认不是抖动
                current_level = GPIO_ReadInputDataBit(port, pin);
                if (current_level == 0) {
                    // 20ms后仍是低电平，确认是真按下
                    rt->state = KEY_STATE_PRESSED;
                    rt->last_level = 0;
                    return 1;  // 上报：该按键被确认按下
                } else {
                    // 20ms后变回高电平了，刚才只是抖动，回到空闲
                    rt->state = KEY_STATE_IDLE;
                }
            }
            break;
            
        /*------------------ 状态3：已确认按下 ------------------*/
        case KEY_STATE_PRESSED:
            // 检测上升沿：上次是低电平(0)，这次变成高电平(1)，说明手指松开了
            if (current_level == 1 && rt->last_level == 0) {
                rt->state = KEY_STATE_RELEASE_DEBOUNCE; // 进入释放消抖阶段
                rt->debounce_time = now;                // 记录松手时刻
            }
            // 如果手指一直按着不放，current_level和last_level都是0，条件不满足，
            // 一直停留在此状态，防止重复触发。这就是"按住只响一次"的秘密。
            break;
            
        /*------------------ 状态4：释放消抖 ------------------*/
        case KEY_STATE_RELEASE_DEBOUNCE:
            // 等待20ms，过滤松手时的机械抖动
            if (now - rt->debounce_time >= 20) {
                rt->state = KEY_STATE_IDLE;  // 消抖完成，回到空闲，等待下一次按键
            }
            break;
    }
    
    // 把当前电平保存为"上次电平"，为下一轮检测边沿做准备
    rt->last_level = current_level;
    return 0;
}

/**
  * @brief  扫描所有按键，将检测到的事件存入缓冲
  * @note   应在while(1)主循环中每轮调用。
  *         若pending_event未被取走，本轮跳过不扫描（防止事件覆盖）。
  */
void Key_Scan(void)
{
    // 如果上次事件还没被主循环取走，先不扫新的，防止覆盖
    if (pending_event != KEY_NONE) return;
    
    // 遍历12个按键，逐个调用DetectSingleKey
    for (uint8_t i = 0; i < 12; i++) {
        if (DetectSingleKey(i)) {
            // 检测到确认按下，把该按键的逻辑编号存入事件缓冲
            pending_event = key_table[i].key_code;
            break;  // 一次只处理一个按键，检测到即退出（人不可能真正同时按两个键）
        }
    }
}

/**
  * @brief  获取按键事件
  * @retval 按键事件编码：0~9（数字）、KEY_CONFIRM（确认）、KEY_BACK（返回）、KEY_NONE（无事件）
  * @note   取出事件后，pending_event自动清空为KEY_NONE。
  *         主循环应在每轮Key_Scan之后调用本函数。
  */
uint8_t Key_GetEvent(void)
{
    uint8_t evt = pending_event;
    pending_event = KEY_NONE;  // 取走即清空，允许下一次事件进入
    return evt;
}
