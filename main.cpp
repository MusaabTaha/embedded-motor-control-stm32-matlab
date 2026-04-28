#include "main.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart1;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);

namespace
{
    // ------------------------------------------------------------
    // 115200-safe outer speed loop only
    // Input from MATLAB : "ref_rpm,w_m_mrad\n"
    // Output to MATLAB  : "t=<mNm>\n"
    // ------------------------------------------------------------

    constexpr float HIL_TS        = 0.01f;   // 10 ms
    constexpr float RAD_S_TO_RPM  = 9.549296585513721f;

    // Conservative outer-loop gains for 10 ms HIL
    constexpr float KP_W          = 0.0020f;
    constexpr float KI_W          = 0.0100f;

    constexpr float W_M_MAX_RPM   = 3000.0f;
    constexpr float T_LIMIT_NM    = 30.0f;

    struct Inputs
    {
        float ref_rpm;
        float w_m;
    };

    float clampf(float x, float lo, float hi)
    {
        if (x < lo) return lo;
        if (x > hi) return hi;
        return x;
    }

    float absf(float x)
    {
        return (x < 0.0f) ? -x : x;
    }

    void uart_send_str(const char* s)
    {
        HAL_UART_Transmit(
            &huart1,
            reinterpret_cast<uint8_t*>(const_cast<char*>(s)),
            static_cast<uint16_t>(strlen(s)),
            100
        );
    }

    int uart_read_line(char* buf, int max_len)
    {
        int i = 0;
        uint8_t c = 0;

        while (true)
        {
            if (HAL_UART_Receive(&huart1, &c, 1, HAL_MAX_DELAY) != HAL_OK)
            {
                continue;
            }

            if (c == '\r')
            {
                continue;
            }

            if (c == '\n')
            {
                buf[i] = '\0';
                return i;
            }

            if (i < (max_len - 1))
            {
                buf[i++] = static_cast<char>(c);
            }
        }
    }

    bool parse_inputs(const char* line, Inputs& in)
    {
        long ref_rpm_i = 0;
        long w_m_mrad  = 0;

        const int n = sscanf(line, "%ld,%ld", &ref_rpm_i, &w_m_mrad);
        if (n != 2)
        {
            return false;
        }

        in.ref_rpm = static_cast<float>(ref_rpm_i);
        in.w_m     = static_cast<float>(w_m_mrad) * 1e-3f;
        return true;
    }

    class SpeedPI
    {
    public:
        float update(float ref_rpm, float w_m_rad_s)
        {
            const float ref_sat_rpm = clampf(ref_rpm, -W_M_MAX_RPM, W_M_MAX_RPM);
            const float w_m_rpm     = w_m_rad_s * RAD_S_TO_RPM;
            const float err_rpm     = ref_sat_rpm - w_m_rpm;

            const bool antiwindup_ok =
                (absf(prev_unsat_) < T_LIMIT_NM) ||
                ((prev_unsat_ * err_rpm) < 0.0f);

            const float err_aw    = antiwindup_ok ? err_rpm : 0.0f;
            const float integ_in  = KI_W * err_aw;

            // trapezoidal integrator
            integ_ += 0.5f * HIL_TS * (integ_in + prev_integ_in_);
            prev_integ_in_ = integ_in;

            const float unsat = integ_ + (KP_W * err_rpm);
            prev_unsat_ = unsat;

            return clampf(unsat, -T_LIMIT_NM, T_LIMIT_NM);
        }

        void reset()
        {
            integ_ = 0.0f;
            prev_integ_in_ = 0.0f;
            prev_unsat_ = 0.0f;
        }

    private:
        float integ_ = 0.0f;
        float prev_integ_in_ = 0.0f;
        float prev_unsat_ = 0.0f;
    };

    SpeedPI g_speed_pi;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();

    uart_send_str("READY\n");

    char line[96];
    char out[64];

    while (1)
    {
        const int n = uart_read_line(line, sizeof(line));
        if (n <= 0)
        {
            continue;
        }

        if (strcmp(line, "PING") == 0)
        {
            uart_send_str("PONG\n");
            HAL_GPIO_TogglePin(GPIOG, LD3_Pin);
            continue;
        }

        if (strcmp(line, "RESET") == 0)
        {
            g_speed_pi.reset();
            uart_send_str("OK\n");
            HAL_GPIO_TogglePin(GPIOG, LD3_Pin);
            continue;
        }

        Inputs in{};
        if (!parse_inputs(line, in))
        {
            uart_send_str("ERR\n");
            continue;
        }

        const float t_ref_nm = g_speed_pi.update(in.ref_rpm, in.w_m);
        const int t_ref_mNm  = static_cast<int>(t_ref_nm * 1000.0f);

        snprintf(out, sizeof(out), "t=%d\n", t_ref_mNm);
        uart_send_str(out);

        HAL_GPIO_TogglePin(GPIOG, LD3_Pin);
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 50;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ = 7;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK
                                | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1
                                | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV8;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOG_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOG, LD3_Pin | LD4_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = LD3_Pin | LD4_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
}

extern "C" void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
