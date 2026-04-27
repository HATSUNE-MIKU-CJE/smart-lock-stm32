#ifndef __DELAY_H
#define __DELAY_H

void Delay_us(uint32_t us);
void Delay_ms(uint32_t ms);
void Delay_s(uint32_t s);

// 新增：系统滴答计时功能
void Systick_Init(void);              // 初始化系统滴答定时器
uint32_t GetTick(void);               // 获取系统运行时间（毫秒）
void Delay_Tick(uint32_t ms);         // 非阻塞延时（基于GetTick）

#endif
