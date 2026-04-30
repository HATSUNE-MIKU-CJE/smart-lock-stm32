# 智能密码锁开发旅途记录

> 这不是项目文档，这是我自己复习用的笔记本。记录每个阶段学了什么、踩了什么坑、当时是怎么想的。
>
>  hardware：STM32F103C8 | 直流减速电机+凸轮 | 微动开关 | 有源蜂鸣器 | 0.96 OLED | 4×3矩阵键盘

---

## 阶段0：开局与硬件选型

### 做了什么

一开始打算用**舵机**驱动锁舌，后来改成了**直流减速电机+凸轮**结构。

**为什么改？**
- 舵机需要精确的角度控制，每把锁的机械结构不同，角度参数得反复调
- 凸轮结构只需要**单向旋转**，转到位触发微动开关就停，不用关心具体转了多少度
- 直流电机驱动更简单：给PWM就转，方向固定，一个IO控制启停

**电机参数**
- TIM1_CH4 (PA11) 输出 PWM
- PSC=71, ARR=99 → 72MHz/72/100 = 10kHz
- 占空比 80%，力气够，噪音小

### 技术栈

- STM32F103C8 时钟树、GPIO 复用
- TIM1 高级定时器 PWM 输出
- 半桥电机驱动模块

### 关键代码

```c
// PWM.c：TIM1 初始化
TIM_TimeBaseInitStructure.TIM_Period = 100 - 1;       // ARR=99
TIM_TimeBaseInitStructure.TIM_Prescaler = 72 - 1;     // PSC=71
// 72MHz → 1MHz → 10kHz

// Motor.c：单向旋转
void Motor_On(void)
{
    GPIO_ResetBits(GPIOB, GPIO_Pin_12);   // IN1=0
    GPIO_SetBits(GPIOB, GPIO_Pin_13);     // IN2=1
    PWM_SetCompare4(80);                   // 80%占空比
}
```

### 拓展思考

**Q：舵机和直流电机到底怎么选？**
> 舵机适合角度精确控制的场景（如云台、机械臂），但需要PWM脉宽调角度，机械结构变了参数就得重调。直流电机+凸轮适合"转到位就停"的场景，控制逻辑简单，成本更低。而且凸轮顶开锁舌后，弹簧的弹力会自己把锁舌弹回去，不需要电机反转关锁。

**Q：为什么用10kHz而不是更高？**
> PWM频率太低电机会嗡嗡响（人耳能听到），太高开关损耗大、驱动芯片发热。10kHz在人耳听觉边缘，既安静又高效。

---

## 阶段1：系统心跳——TIM4时基

### 做了什么

整个系统需要一个"时钟"，用来：
- 按键消抖计时（20ms）
- 开锁保持延时（5s）
- 关锁转动延时（500ms）
- 配置模式超时（10s）

不能再用 `Delay_ms()` 阻塞了，否则按键按了系统没空理你。

### 技术栈

- TIM4 定时器中断
- 全局 volatile 计数器
- 非阻塞延时思想

### 关键代码

```c
// Delay.c
volatile uint32_t systick_counter = 0;

void TIM4_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET)
    {
        systick_counter++;  // 每1ms加1
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
    }
}

void Systick_Init(void)
{
    // TIM4, PSC=71, ARR=999 → 1ms中断
    TIM_TimeBaseInitTypeDef TIM_InitStruct;
    TIM_InitStruct.TIM_Period = 1000 - 1;
    TIM_InitStruct.TIM_Prescaler = 72 - 1;
    // ... NVIC配置等
}

uint32_t GetTick(void)
{
    return systick_counter;  // 获取当前系统时间（ms）
}

// 使用方式：非阻塞延时
uint32_t start = GetTick();
while (GetTick() - start < 5000) { /* 干别的 */ }
```

### 拓展思考

**Q：为什么用TIM4而不用SysTick？**
> SysTick是ARM内核的，有些RTOS会占用。TIM4是外设，独立可控，而且中断优先级可以灵活配置。关键是——江协科技的例程里用的就是TIM4，跟着走不容易踩坑。

**Q：volatile不能忘**
> `systick_counter` 必须加 `volatile`，否则编译器优化后，主循环里读到的值永远是旧的。中断里改、主循环里读，这是经典的编译器优化陷阱。

---

## 阶段2：人机交互——OLED显示

### 做了什么

- 0.96寸 OLED，SSD1306控制器，PB8/PB9软件模拟I2C
- 支持UTF-8中文显示（江协科技V2.0库）
- 解决了**星号闪烁**问题

**闪烁问题的根因**：
- 一开始每轮循环都 `OLED_Clear()` 再重画，导致星号一闪一闪
- 解决：只在**状态切换时清屏**，状态内只刷新变化区域（`OLED_ClearArea`）

### 技术栈

- SSD1306 指令集
- 软件模拟 I2C 时序
- 局部刷新策略

### 关键代码

```c
// 错误做法：每轮都清屏，导致闪烁
while (1) {
    OLED_Clear();              // ❌ 闪！
    OLED_ShowString(...);      //
    OLED_Update();             //
}

// 正确做法：状态切换时清屏，状态内局部刷新
case STATE_PASSWORD_INPUT:
    if (Password_InputDigit(evt))
    {
        uint8_t len = Password_GetInputLen();
        OLED_ClearArea(0, 16, 128, 16);  // 只清第二行
        for (uint8_t i = 0; i < len; i++)
        {
            OLED_ShowString(i * 8, 16, "*", OLED_8X16);
        }
        OLED_Update();  // 统一刷新一次
    }
    break;
```

### 拓展思考

**Q：软件I2C和硬件I2C哪个好？**
> 软件I2C慢点但引脚任意选，硬件I2C快点但引脚固定（PB6/PB7）。本项目对速率没要求，软件I2C够用了，而且PB6/PB7可能被别的外设占用。不过软件I2C占CPU，以后如果加很多功能可能要改成硬件的。

---

## 阶段3：输入系统——按键消抖

### 做了什么

- 独立按键版本：12路按键各接一根GPIO，上拉输入
- 状态机消抖：IDLE → PRESS_DEBOUNCE → PRESSED → RELEASE_DEBOUNCE → IDLE
- 20ms消抖阈值，基于 `GetTick()` 非阻塞实现
- 事件缓冲机制：`Key_Scan()` 扫描 → `Key_GetEvent()` 取事件
- 后来升级为 **4×3矩阵键盘**，行列扫描方式

### 技术栈

- GPIO 上拉输入 / 推挽输出
- 状态机设计模式
- 矩阵键盘行列扫描
- 事件驱动架构

### 关键代码

**独立按键消抖核心**：
```c
typedef enum {
    KEY_STATE_IDLE,
    KEY_STATE_PRESS_DEBOUNCE,   // 按下消抖
    KEY_STATE_PRESSED,          // 确认按下
    KEY_STATE_RELEASE_DEBOUNCE  // 释放消抖
} KeyState_t;

// 状态机转换
switch (rt->state) {
    case KEY_STATE_IDLE:
        if (current_level == 0 && rt->last_level == 1) {
            rt->state = KEY_STATE_PRESS_DEBOUNCE;
            rt->debounce_time = now;
        }
        break;
    case KEY_STATE_PRESS_DEBOUNCE:
        if (now - rt->debounce_time >= 20) {
            if (current_level == 0) {
                rt->state = KEY_STATE_PRESSED;
                return 1;  // 上报事件
            } else {
                rt->state = KEY_STATE_IDLE;  // 抖动，回 idle
            }
        }
        break;
    // ...
}
```

**矩阵键盘扫描核心**：
```c
static int8_t Matrix_ScanRaw(void)
{
    for (uint8_t r = 0; r < 4; r++) {
        // 所有行置高
        for (uint8_t i = 0; i < 4; i++) GPIO_SetBits(row_ports[i], row_pins[i]);
        // 当前行置低
        GPIO_ResetBits(row_ports[r], row_pins[r]);
        Delay_us(10);  // 等电平稳定
        // 读列
        for (uint8_t c = 0; c < 3; c++) {
            if (GPIO_ReadInputDataBit(col_ports[c], col_pins[c]) == 0)
                return r * 3 + c;
        }
    }
    return -1;
}
```

**事件编码**：
```c
#define KEY_NONE     0xFF   // 无事件
#define KEY_CONFIRM  0xFE   // * 键（确认）
#define KEY_BACK     0xFD   // # 键（返回/退格）
// 0x00~0x09：数字键
```

### 拓展思考

**Q：为什么不用Delay_ms消抖？**
> `Delay_ms(20)` 会阻塞20ms，这期间系统什么都干不了。如果用在中断里，还会卡死整个系统。用 `GetTick()` 记录时间戳，主循环继续跑，到时间了再检查，这是非阻塞的思想，嵌入式里到处都在用。

**Q：矩阵键盘和独立按键的本质区别？**
> 独立按键每个键一根GPIO，直接读电平就行，简单但占引脚。矩阵键盘用行列交叉，N×M个键只需要N+M根线，省引脚但需要扫描。消抖逻辑是一样的，只是"读电平"变成了"行列扫描推断电平"。

**Q：怎么判断行列有没有接反？**
> 临时把显示改成数字而不是星号，按一圈键盘就知道哪个键对应什么值了。或者拿万用表通断档，按住一个键测哪两根线导通。

---

## 阶段4：听觉反馈——蜂鸣器

### 做了什么

PB10 驱动有源蜂鸣器（低电平导通）。不同操作配不同提示音，让用户不用看屏幕也知道发生了什么。

| 事件 | 节奏 |
|------|------|
| 按键按下 | 单短音 ~30ms |
| 开锁成功 | 短-长-短 三声 |
| 密码错误 | 双短音 |
| 关锁完成 | 短-短-长 三声 |
| 退格 | 中音 ~100ms |

### 技术栈

- GPIO 控制有源蜂鸣器
- 非阻塞时基实现节奏控制（也可以简单用 Delay_ms，因为蜂鸣器响的时候系统本来就没别的事）

### 关键代码

```c
void Buzzer_KeyPress(void)  { Buzzer_On(); Delay_ms(30);  Buzzer_Off(); }
void Buzzer_Backspace(void) { Buzzer_On(); Delay_ms(100); Buzzer_Off(); }

void Buzzer_Unlock(void)
{
    // 短-长-短
    Buzzer_On(); Delay_ms(100); Buzzer_Off(); Delay_ms(50);
    Buzzer_On(); Delay_ms(300); Buzzer_Off(); Delay_ms(50);
    Buzzer_On(); Delay_ms(100); Buzzer_Off();
}

void Buzzer_Error(void)
{
    // 短叫两声
    Buzzer_On(); Delay_ms(80); Buzzer_Off(); Delay_ms(80);
    Buzzer_On(); Delay_ms(80); Buzzer_Off();
}
```

### 拓展思考

**Q：有源蜂鸣器和无源蜂鸣器区别？**
> 有源蜂鸣器内部带振荡电路，给电就响，控制简单（一个GPIO高低电平）。无源蜂鸣器需要外部给方波，频率决定音调，可以播放音乐但控制复杂。本项目只需要"响/不响"，有源够了。

---

## 阶段5：安全核心——密码模块

### 做了什么

- 6位数字密码，逐位数组比较
- 输入缓存 `s_input[6]` + 长度计数器 `s_len`
- 满6位后拒绝继续录入（超位不录）
- 退格功能：带下溢保护（`s_len > 0` 才减）
- 暴露 `Password_SetPassword()` 和 `Password_GetInputArray()` 接口供 Flash/Config 模块调用

### 技术栈

- 数组操作与边界检查
- 模块化封装（static 隐藏内部状态，暴露必要接口）

### 关键代码

```c
static uint8_t s_password[6];   // 当前有效密码（RAM中，从Flash加载）
static uint8_t s_input[6];      // 用户输入缓存
static uint8_t s_len = 0;       // 当前输入长度

uint8_t Password_InputDigit(uint8_t digit)
{
    if (s_len >= 6) return 0;   // 满6位，拒绝录入
    s_input[s_len++] = digit;
    return 1;
}

uint8_t Password_Check(void)
{
    if (s_len != 6) return 0;
    for (uint8_t i = 0; i < 6; i++)
        if (s_input[i] != s_password[i]) return 0;
    return 1;
}

// 给外部模块用的接口（如Storage加载密码、Config修改密码）
void Password_SetPassword(const uint8_t* pwd)
{
    for (uint8_t i = 0; i < 6; i++) s_password[i] = pwd[i];
}

const uint8_t* Password_GetInputArray(void)
{
    return s_input;  // Config模块读取用户输入的6位数字
}
```

### 拓展思考

**Q：为什么不把 s_password 暴露出去？**
> `static` 是模块的围墙。外部只通过接口操作，内部怎么实现随便改。如果直接暴露数组，以后改密码长度、改加密方式，所有用到的地方都要改。接口就是合约，内部实现是秘密。

**Q：为什么要暴露 GetInputArray？**
> 配置模式需要保存用户输入的6位新密码，但 `s_input` 是 static 的。本来不想暴露，但配置模式确实需要读输入缓存。折中方案：暴露一个只读的 `const uint8_t*` 指针，外部能读不能写，安全。

---


## 阶段6：执行机构——开锁/关锁时序

### 做了什么

凸轮结构的完整开锁流程：
1. 电机启动，凸轮顶开锁舌，压缩弹簧
2. 凸轮顶点触发微动开关（到位检测）
3. 电机停止，凸轮卡住锁舌，保持开锁状态
4. 延时5秒
5. 电机继续同向旋转，凸轮脱离锁舌
6. 弹簧弹力驱动锁舌自动复位（关锁）
7. 延时500ms后电机停止

**关键设计**：不需要电机反转！凸轮转一圈，先顶开再脱开，弹簧自动复位。

### 技术栈

- TIM1 PWM 调速
- GPIO 方向控制
- 非阻塞延时状态机
- 微动开关反馈

### 关键代码

```c
// Unlock.c：内部维护4阶段子状态机

typedef enum {
    PHASE_IDLE = 0,
    PHASE_UNLOCKING,   // 电机转，等开关到位
    PHASE_UNLOCKED,    // 到位停止，等5秒
    PHASE_LOCKING      // 继续转500ms脱开
} UnlockPhase_t;

static UnlockPhase_t s_phase = PHASE_IDLE;
static uint32_t s_tick = 0;

void Unlock_Start(void)
{
    Motor_On();
    s_phase = PHASE_UNLOCKING;
}

void Unlock_Tick(void)
{
    switch (s_phase) {
        case PHASE_UNLOCKING:
            if (Switch_IsClosed()) {
                Motor_Off();
                s_tick = GetTick();
                s_phase = PHASE_UNLOCKED;
                OLED_ShowString(0, 0, "OPEN", OLED_8X16);
            }
            break;
        case PHASE_UNLOCKED:
            if (GetTick() - s_tick > 5000) {
                Motor_On();
                s_tick = GetTick();
                s_phase = PHASE_LOCKING;
            }
            break;
        case PHASE_LOCKING:
            if (GetTick() - s_tick > 500) {
                Motor_Off();
                s_phase = PHASE_IDLE;
                OLED_ShowString(0, 0, "CLOSED", OLED_8X16);
                Buzzer_Lock();
            }
            break;
    }
}
```

### 拓展思考

**Q：为什么不需要电机反转？**
> 凸轮是个偏心圆，转一圈先顶开锁舌（开锁），继续转就脱开了（关锁）。弹簧始终顶着锁舌，凸轮一脱开，弹簧自动把锁舌弹回去。比正反转电机少一半控制逻辑，而且不用考虑堵转电流。

**Q：500ms关锁时间怎么定的？**
> 凭经验先试200ms，发现凸轮还没完全脱开；试1000ms，电机空转太久浪费电还吵。500ms是实际调出来的，刚好凸轮脱开、弹簧复位。

---

## 阶段7：闭环控制——微动开关

### 做了什么

- PA12 上拉输入，检测凸轮是否顶到位
- 5ms连续采样消抖，防止机械抖动导致误判
- 提供 `Switch_IsClosed()` 接口

### 技术栈

- GPIO 输入 + 内部上拉
- 软件消抖（连续采样）

### 关键代码

```c
// Switch.c
uint8_t Switch_IsClosed(void)
{
    // 连续5次读到低电平才认为是真闭合
    for (uint8_t i = 0; i < 5; i++) {
        if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_12) != 0)
            return 0;  // 有一次是高，说明没到位
        Delay_ms(1);   // 间隔1ms采样
    }
    return 1;  // 连续5次低电平，确认到位
}
```

### 拓展思考

**Q：微动开关和到位检测的关系？**
> 微动开关是"触觉"，告诉MCU"凸轮已经顶到最高点了"。没有这个反馈，电机不知道要停，会一直转。这是闭环控制的核心：执行器（电机）+ 传感器（开关）+ 控制器（MCU）。

---

## 阶段8：掉电保持——Flash存储

### 做了什么

密码存在 Flash 最后一页 `0x0800FC00`，掉电不丢失。

**数据结构**：12字节紧凑结构体
```c
typedef struct {
    uint32_t magic;      // 0xA5A5A5A5，识别是否已初始化
    uint8_t  pwd[6];     // 6位密码
    uint16_t checksum;   // 前10字节累加和
} LockConfig_t;
```

**写入流程**：解锁Flash → 擦除整页（1KB）→ 按字写入（3个uint32_t）→ 上锁Flash

**读取流程**：读Flash地址 → 检查魔数 → 校验和验证 → 合法则加载到RAM，不合法则恢复默认密码

### 技术栈

- STM32 Flash 标准库编程
- 页擦除、字写入
- 校验和机制
- 结构体内存布局（`#pragma pack` 防止编译器填充）

### 关键代码

```c
// Store.c：写入密码到Flash
uint8_t Storage_WritePassword(const uint8_t* newPwd)
{
    LockConfig_t cfg;
    cfg.magic = FLASH_MAGIC_NUM;
    for (uint8_t i = 0; i < 6; i++) cfg.pwd[i] = newPwd[i];
    cfg.checksum = CalcChecksum(&cfg);  // 计算校验和
    
    FLASH_Unlock();
    
    if (FLASH_ErasePage(FLASH_STORAGE_ADDR) != FLASH_COMPLETE) {
        FLASH_Lock();
        return 0;
    }
    
    uint32_t* pWord = (uint32_t*)&cfg;
    for (uint8_t i = 0; i < 3; i++) {
        if (FLASH_ProgramWord(FLASH_STORAGE_ADDR + i*4, pWord[i]) != FLASH_COMPLETE) {
            FLASH_Lock();
            return 0;
        }
    }
    
    FLASH_Lock();
    return 1;
}
```

### 拓展思考

**Q：为什么必须先擦除再写入？**
> Flash 的物理特性：只能把 1 改成 0，不能从 0 变回 1。擦除整页把所有位变成 1（0xFF），之后才能按需要把某些位写成 0。如果不擦除直接写，数据会乱。

**Q：为什么选最后一页？**
> 程序代码存在 Flash 前面，选最后一页（0x0800FC00）不会和程序冲突。64KB Flash = 0x08000000~0x0800FFFF，末页起始地址 = 0x08010000 - 0x400 = 0x0800FC00。

**Q：魔数和校验和有什么区别？**
> 魔数告诉你"这地方存过密码"，校验和告诉你"数据没被损坏"。魔数不能防位翻转（电磁干扰可能让某一位变了，但魔数碰巧还是对的），校验和能发现数据变化。两个一起用才靠谱。

**Q：密码忘了怎么办？**
> Keil里勾选"Erase Full Chip"全片擦除再烧录，或者临时在代码里强制写一遍默认密码。全片擦除后上电，Storage_Init发现魔数不对，自动恢复123456。

---

## 阶段9：用户配置——修改密码

### 做了什么

待机界面按 `#`（返回键）进入配置模式：
1. **验证旧密码**：输入当前密码，错误直接退出
2. **输入新密码**：输满6位
3. **再次确认**：输第二次，逐位比对
4. **写入Flash**：两次一致才写入，不一致提示 Mismatch

**安全设计**：
- 10秒超时：最后一次按键后10秒无操作自动退出
- 有按键操作时刷新超时计时器（不是进入配置后固定10秒）
- 旧密码错误不返回配置入口，直接回待机（防暴力尝试）

### 技术栈

- 多级状态机
- 超时机制
- Flash写入
- 输入缓存读取（`Password_GetInputArray()`）

### 关键代码

```c
// Config.c：配置模式状态机（节选）

// 第1步：验证旧密码
if (Password_Check()) {
    Password_Reset();
    s_phase = CFG_PHASE_SET;  // 进入输入新密码
} else {
    Buzzer_Error();
    ExitConfig();  // 直接退出，不给你再试
}

// 第2步：保存第一次输入的新密码
const uint8_t* input = Password_GetInputArray();
for (uint8_t i = 0; i < 6; i++) s_newPwd[i] = input[i];

// 第3步：确认新密码，逐位比对
uint8_t match = 1;
for (uint8_t i = 0; i < 6; i++) {
    if (input[i] != s_newPwd[i]) { match = 0; break; }
}

if (match) {
    Storage_WritePassword(s_newPwd);
    Password_SetPassword(s_newPwd);
    OLED_ShowString(0, 0, "Saved", OLED_8X16);
} else {
    OLED_ShowString(0, 0, "Mismatch", OLED_8X16);
}
```

### 拓展思考

**Q：为什么要输两次新密码？**
> 防止手滑。只输一次，按错一个数字就完了，密码变成自己都不知道的数。输两次确认，和注册账号时的"确认密码"是一个道理。

**Q：超时为什么要刷新而不是固定计时？**
> 最早写的是进入配置后固定10秒退出，结果用户输得慢，还没输完就退出了。改成"最后一次按键后10秒"，只要你在操作，计时器就不断刷新。这才是人性化的设计。

---

## 阶段10：架构优化——模块拆分

### 做了什么

一开始所有状态机全塞在 `main.c` 里，400多行，滚半天鼠标。后来拆了：
- `Unlock.c/h`：开锁/保持/关锁的自动时序
- `Config.c/h`：配置模式的完整流程
- `main.c` 只负责初始化 + 事件分发

**主循环调度逻辑**：
```c
while (1) {
    Key_Scan();
    uint8_t evt = Key_GetEvent();
    
    if (evt != KEY_NONE) {
        Buzzer_KeyPress();
        if (Config_IsActive()) Config_ResetTimeout();
    }
    
    Unlock_Tick();  // 开锁流程独立驱动
    
    if (Config_IsActive()) {
        Config_HandleEvent(evt);  // 配置模式独占处理
        continue;
    }
    
    if (Unlock_IsBusy()) continue;  // 开锁过程中不处理待机/密码输入
    
    switch (g_state) {
        case STATE_IDLE:           // 待机 + 进配置入口
        case STATE_PASSWORD_INPUT: // 密码输入
    }
}
```

### 技术栈

- 模块化设计
- 状态机分层
- 接口抽象

### 拓展思考

**Q：模块拆分的原则是什么？**
> 一个模块只做一件事。Unlock只管电机时序，Config只管改密码，Password只管密码校验，Store只管Flash读写。模块之间通过接口通信，不直接访问对方的内部数据。

**Q：main.c应该有多薄？**
> 理想情况下，main.c只负责：初始化 → 主循环 → 事件分发。具体的业务逻辑由各模块实现。如果main.c超过200行，就该想想是不是有些东西该拆出去了。

---

## 附录A：踩坑清单

| 问题 | 现象 | 原因 | 解决 |
|------|------|------|------|
| OLED星号闪烁 | 输入数字时星号一闪一闪 | 每轮循环都OLED_Clear()全屏刷新 | 状态切换时清屏，状态内只刷新变化区域 |
| 配置模式莫名退出 | 输到一半自动回待机 | 超时计时器只在进入配置时设置，不按按键也倒计时 | 任意按键操作时刷新g_configTick |
| 矩阵键盘#和0反了 | 按#出0，按0退格 | key_code_map映射错误 / 列线接反 | 先确认是软件映射问题还是硬件接线问题，再针对性修正 |
| 按键没反应 | 按了键盘没反应 | 矩阵键盘行列线接错 / 扫描后没延时电平不稳 | 用万用表测行列关系，扫描后加Delay_us(10) |
| 密码断电丢失 | 重启后密码恢复123456 | 没调Storage_Init / Flash写入失败 | 确认stm32f10x_flash.h已包含，Storage_Init在Password_Init之前调用 |

## 阶段11：低功耗——PWR停止模式

### 做了什么

IDLE状态下3分钟无操作，系统自动进入 **Stop模式**，功耗从几十mA降到几十μA。只有按 `*` 键才能唤醒，其他键按了继续睡（软件过滤）。

**为什么选Stop而不是Sleep或Standby？**

| 模式 | CPU | 时钟 | SRAM | 唤醒 | 功耗 |
|------|-----|------|------|------|------|
| Sleep | 停 | 外设继续跑 | 保留 | 任意中断 | ~10mA |
| **Stop** | **停** | **全停** | **保留** | **EXTI中断** | **~30~50μA** |
| Standby | 断电 | 全停 | **丢失** | 复位 | ~2~5μA |

> Stop是sweet spot：功耗降三个数量级，SRAM数据全在（密码不丢），EXTI按键唤醒。

**唤醒机制**：
1. 矩阵键盘的3根列线（PA4~PA6）配置为EXTI下降沿中断
2. 任意键按下都会触发EXTI，唤醒CPU
3. 唤醒后`SystemInit()`恢复72MHz时钟
4. 矩阵扫描确认是不是*键，不是就继续睡

**为什么用"软件过滤"而不是硬件精确唤醒？**

矩阵键盘的行列共享特性决定了：单独检测*键需要额外飞线。务实的做法是列线全部配EXTI，醒来扫一眼，不是*键翻个身继续睡。整个过程不到1ms，用户无感知。

### 技术栈

- PWR停止模式（`PWR_EnterSTOPMode`）
- EXTI外部中断（AFIO映射 + NVIC配置）
- `SystemInit()`时钟恢复
- 内层while循环软件过滤

### 关键代码

```c
// 进入Stop模式（WFI = Wait For Interrupt）
PWR_EnterSTOPMode(PWR_Regulator_ON, PWR_STOPEntry_WFI);

// ========== 唤醒后从这里继续 ==========
SystemInit();        // 恢复72MHz（Stop关闭了HSE/PLL）
Delay_ms(20);        // 等时钟稳定 + 按键消抖

Key_Scan();
if (Key_GetEvent() == KEY_CONFIRM)  // 是*键？
    break;          // 退出内层循环，正常开机
// 不是*键，继续睡（内层while循环）
```

**状态切换检测（防止漏重置计时）**：

```c
// 在while(1)开头，switch之前
if (g_state != g_lastState)
{
    if (g_state == STATE_IDLE)
        g_idleTick = GetTick();  // 刚进入IDLE，重置超时计时
    g_lastState = g_state;
}
```

### 拓展思考

**Q：为什么唤醒后必须调SystemInit()？**
> Stop模式下HSE（外部晶振）和PLL（锁相环）都被关了。唤醒后默认用内部HSI（8MHz）跑，不调SystemInit()的话系统慢得像蜗牛，TIM4时基也不对。

**Q：Stop期间TIM4停了，systick_counter还准吗？**
> Stop期间TIM4不计数，systick_counter暂停。唤醒后TIM4继续从停下来的地方计数，时间差计算是正确的。因为debounce_time记录的是暂停前的值，唤醒后继续累加。

**Q：电机正在转的时候能进Stop吗？**
> 不能，而且代码里也不会。Stop只在STATE_IDLE时判断，而STATE_IDLE意味着开锁流程已经结束（Unlock_IsBusy()为0）。电机转的时候主循环在等Switch_IsClosed()或等延时，不会走到STATE_IDLE。

**Q：3分钟会不会太长/太短？**
> 测试时用20秒，正式用3分钟。太短了用户输个密码要慌，太长了电池耗得快。3分钟是个合理的折中——正常人站在锁前不会愣3分钟不操作。

---

## 附录B：待办事项

- [x] 密码掉电保存
- [x] 配置模式改密码
- [x] 矩阵键盘替换独立按键
- [x] 低功耗睡眠模式（PWR）
- [ ] 指纹模块（AS608）
- [ ] 语音播报
- [ ] 蓝牙/WiFi远程开锁

---

> 最后更新：2026年4月
> 记住：看懂代码不如看懂思路，抄代码不如懂原理。
