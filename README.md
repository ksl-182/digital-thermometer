# 数字温度计（STM32）

电子技术课程设计 — 基于 STM32F103 的数字温度计：LM35 测温、四位数码管显示、超温声光指示，并提供 Proteus 仿真工程。

| | |
|---|---|
| **项目名称** | 数字温度计课程设计 |
| **专业** | 自动化 |
| **组长** | 024301734106 柯善隆 |
| **组员** | 023301915206 郝周雄 |
| **指导教师** | 张珍云 |
| **开发环境** | VS Code + PlatformIO 插件 |
| **仿真** | Proteus 9.0 |

## 功能

- LM35 模拟温度采集（10 mV/°C），12-bit ADC，16 次滑动平均
- 四位共阴数码管动态扫描显示 `xx.x`（摄氏度），右对齐、空白消隐防残影
- 上电自检显示 `8888`（约 0.5 s）
- 超温报警：≥ 35 °C 红灯亮、绿灯灭；正常时绿灯亮
- 负温显示（`-xx.x`），量程约 −9.9 ~ 99.9 °C

## 硬件连接

| 引脚 | 连接 | 说明 |
|------|------|------|
| PA0 | LM35 VOUT | 模拟输入 |
| PA3 | 绿色 LED | 正常指示（高电平点亮） |
| PA4 | 红色 LED | 报警指示（高电平点亮） |
| PB0–PB7 | 数码管段选 a–dp | 共阴极段码 |
| PB8–PB11 | 数码管位选 1–4 | 低电平选通 |

> Proteus 仿真中 STM32 使用内部 HSI 8 MHz，无外部晶振；LM35 按 VDDA ≈ 5 V 标定（见 `src/main.cpp` 中 `ADC_VREF_MV`）。

## 软件结构

```
fire/
├── src/
│   └── main.cpp        # 测温、显示扫描、报警逻辑
├── platformio.ini      # PlatformIO 工程配置
├── include/
├── lib/                # 本地库（OneWire / DallasTemperature，可选用）
└── test/
```

核心逻辑：

1. `adc_read_avg()` — 长采样 + 16 次平均，提高 Proteus 稳定性  
2. `read_temp_c()` — ADC → mV → °C（LM35：10 mV/°C）  
3. `updateBuffer()` — 温度格式化为四位段码（含小数点、负号、消隐）  
4. `scan_once()` — 动态扫描，先关位选再改段码，避免鬼影  

## 快速开始

### 编译（PlatformIO / VS Code）

1. 安装 [VS Code](https://code.visualstudio.com/) 与 **PlatformIO IDE** 插件  
2. 打开本项目文件夹  
3. 编译生成固件：

```bash
pio run
```

产物在 `.pio/build/genericSTM32F103C8/` 下的 `firmware.elf` / `firmware.bin` / `firmware.hex`。

### Proteus 仿真

1. 打开仿真工程（见课程提交包中的 `fire_sim`）  
2. 将 U1（STM32F103C8）的固件路径指向上一步编译出的 `firmware.hex` 或 `firmware.bin`  
3. 运行仿真：数码管显示当前温度；改变 LM35 输入电压即可观察读数与报警  

### 真实硬件（可选）

- 使用 ST-Link / 串口下载到 STM32F103C8T6（Blue Pill）  
- 按上表接线；LM35 供电与 ADC 参考需与 `ADC_VREF_MV` 一致（3.3 V 系统请改为 `3300`）  

## 可调参数

| 宏 / 常量 | 默认值 | 含义 |
|-----------|--------|------|
| `TEMP_ALARM` | `35.0f` | 报警阈值（°C） |
| `ADC_VREF_MV` | `5000.0f` | ADC 参考电压（mV） |
| `LM35_PIN` | `PA0` | 温度传感器引脚 |
| `GREEN_LED` / `RED_LED` | `PA3` / `PA4` | 指示灯引脚 |

## 说明

- 本仓库为课程设计**固件源码**（PlatformIO 工程）。  
- 仿真工程、项目报告与答辩 PPT 见课程提交包。  
- 工具链：Arduino 框架（PlatformIO `ststm32`）。

## License

仅供课程学习与交流使用。
