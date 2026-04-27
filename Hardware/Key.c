#include "stm32f10x.h"
#include "Delay.h"

typedef enum{
    KEY_IDLE,
    KEY_PRESS_DEBOUNCE,
    KEY_PRESSED,
    KEY_RELEASE_DEBOUNCE
}KeyState;


static KeyState confirm_state;           // 当前在哪个消抖阶段
static uint8_t last_confirm_state;       // 上一次读到的电平
static uint32_t confirm_debounce_time;   // 记录进入消抖时刻

// 第1张表：按键的"硬件身份"（接在哪个引脚，代表什么含义）
typedef struct {
    GPIO_TypeDef* port;     // GPIO端口，如GPIOA、GPIOB
    uint16_t pin;           // 引脚号，如GPIO_Pin_0
    uint8_t  key_code;      // 这个按键的"编号"：0~9是数字，10是确认，11是返回
} KeyHardware_t;

// 第2张表：按键的"运行时状态"（消抖过程中会变）
typedef struct {
    KeyState state;         // 当前在IDLE、PRESS_DEBOUNCE、PRESSED、RELEASE哪个阶段
    uint8_t  last_level;    // 上次扫描读到的电平
    uint32_t debounce_time; // 进入消抖时的时间戳
} KeyRuntime_t;

// 12个按键的硬件信息（这张表是固定的，只读）
const KeyHardware_t key_table[12] = {
    {GPIOA, GPIO_Pin_0,  1},   // 数字1
    {GPIOA, GPIO_Pin_1,  2},   // 数字2
    {GPIOA, GPIO_Pin_2,  3},   // 数字3
    {GPIOA, GPIO_Pin_3,  4},   // 数字4
    {GPIOA, GPIO_Pin_4,  5},   // 数字5
    {GPIOA, GPIO_Pin_5,  6},   // 数字6
    {GPIOA, GPIO_Pin_6,  7},   // 数字7
    {GPIOA, GPIO_Pin_7,  8},   // 数字8
    {GPIOB, GPIO_Pin_0,  9},   // 数字9
    {GPIOB, GPIO_Pin_1,  10},  // 确认键
    {GPIOB, GPIO_Pin_10, 11},  // 返回键
};

// 12个按键的运行时状态（这些会变，需要记住）
KeyRuntime_t key_runtime[12];


