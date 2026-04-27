#include "stm32f10x.h"

/**
  * @brief  微秒级延时
  * @param  xus 延时时长，范围：0~233015
  * @retval 无
  */
void Delay_us(uint32_t xus)
{
	SysTick->LOAD = 72 * xus;				//设置定时器重装值
	SysTick->VAL = 0x00;					//清空当前计数值
	SysTick->CTRL = 0x00000005;				//设置时钟源为HCLK，启动定时器
	while(!(SysTick->CTRL & 0x00010000));	//等待计数到0
	SysTick->CTRL = 0x00000004;				//关闭定时器
}

/**
  * @brief  毫秒级延时
  * @param  xms 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_ms(uint32_t xms)
{
	while(xms--)
	{
		Delay_us(1000);
	}
}
 
/**
  * @brief  秒级延时
  * @param  xs 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_s(uint32_t xs)
{
	while(xs--)
	{
		Delay_ms(1000);
	}
}

//新增，系统滴答计时功能
volatile uint32_t systick_counter = 0;

/**
  * @brief  TIM4中断服务函数（代替SysTick）
  * @param  无
  * @retval 无
  */
void TIM4_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
        systick_counter++;
    }
}

/**
  * @brief  初始化定时器作为系统滴答（1ms中断）
  * @param  无
  * @retval 无
  */
void Systick_Init(void) {
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    
    // 使能TIM4时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    
    // 1ms中断一次
    // 72MHz系统时钟，预分频72，得到1MHz计数频率
    // 自动重装载值1000，所以1000次计数 = 1ms
    TIM_TimeBaseStructure.TIM_Period = 1000 - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = 72 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);
    
    // 使能更新中断
    TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);
    
    // 配置中断优先级（设置为较低的优先级）
    NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    // 启动定时器
    TIM_Cmd(TIM4, ENABLE);
    
    systick_counter = 0;
}

/**
  * @brief  获取系统运行时间（毫秒）
  * @param  无
  * @retval 系统运行时间（毫秒）
  */
uint32_t GetTick(void) {
    return systick_counter;
}


/**
  * @brief  非阻塞延时
  * @param  ms: 要延时的毫秒数
  * @retval 无
  */
void Delay_Tick(uint32_t ms) {
    uint32_t start_tick = GetTick();
    while ((GetTick() - start_tick) < ms) {
        // 空循环，等待时间到达
    }
}
