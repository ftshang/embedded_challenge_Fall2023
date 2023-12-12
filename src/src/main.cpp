#include <mbed.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "drivers/LCD_DISCO_F429ZI.h"

#define OUT_X_L 0x28
#define CTRL_REG1 0x20
#define CTRL_REG1_CONFIG 0b01'10'1'1'1'1
#define CTRL_REG4 0x23
#define CTRL_REG4_CONFIG 0b0'0'01'0'00'0
#define SPI_FLAG 1
#define SCALING_FACTOR (17.5f * 0.017453292519943295769236907684886f / 1000.0f);
#define RADIUS (0.4572)

SPI spi(PF_9, PF_8, PF_7, PC_1, use_gpio_ssel);
int8_t write_buf[32];
int8_t read_buf[32];
int16_t data_buf[40];
float linear_velocity[40];
Ticker t;
DigitalIn button(PA_0);
EventFlags flags;

// SPI callback used for transfer function.
void spi_cb(int event)
{
    flags.set(SPI_FLAG);
}

// Ticker callback.
void cb()
{
    flags.set(SPI_FLAG);
}

void setupGyroscope()
{
    spi.format(8, 3);
    spi.frequency(1'000'000);
    write_buf[0] = CTRL_REG1;
    write_buf[1] = CTRL_REG1_CONFIG;
    spi.transfer(write_buf, 2, read_buf, 2, spi_cb, SPI_EVENT_COMPLETE);
    flags.wait_all(SPI_FLAG);

    write_buf[0] = CTRL_REG4;
    write_buf[1] = CTRL_REG4_CONFIG;
    spi.transfer(write_buf, 2, read_buf, 2, spi_cb, SPI_EVENT_COMPLETE);
    flags.wait_all(SPI_FLAG);
}

void readData()
{
    write_buf[0] = OUT_X_L | 0x80 | 0x40;
    spi.transfer(write_buf, 7, read_buf, 7, spi_cb, SPI_EVENT_COMPLETE);
    flags.wait_all(SPI_FLAG);
}

int main()
{
    LCD_DISCO_F429ZI lcd;
    lcd.Clear(LCD_COLOR_BLACK);
    lcd.SetTextColor(LCD_COLOR_RED);
    lcd.SetBackColor(LCD_COLOR_BLACK);
    lcd.DisplayStringAtLine(0, (uint8_t *)"Welcome.");
    lcd.DisplayStringAtLine(1, (uint8_t *)"Please press the blue");
    lcd.DisplayStringAtLine(2, (uint8_t *)"button to begin.");

    bool startMenu = true;

    while (startMenu)
    {
        if (button.read() == 1)
        {
            startMenu = false;
        }
    }

    int count = 0;
    int16_t raw_gz;
    float gz;
    t.attach(&cb, 500ms);
    setupGyroscope();

    lcd.Clear(LCD_COLOR_BLACK);
    lcd.SetTextColor(LCD_COLOR_RED);
    lcd.SetBackColor(LCD_COLOR_BLACK);
    lcd.DisplayStringAtLine(0, (uint8_t *)"Begin walking now.");

    while (1)
    {
        flags.wait_all(SPI_FLAG);

        if (count == 40)
        {
            break;
        }
        readData();
        raw_gz = (((uint16_t)read_buf[6]) << 8) | ((uint16_t)read_buf[5]);
        gz = ((float)raw_gz) * SCALING_FACTOR;
        data_buf[count] = raw_gz;
        linear_velocity[count] = gz;
        count += 1;
    }

    // Convert angular velocity into linear velocity.
    float distance = 0.0f;
    for (int i = 0; i < 40; i += 1)
    {
        if (abs(data_buf[i]) > 500)
        {
            distance += (abs(linear_velocity[i]) * RADIUS);
        }
    }

    char distance_str[20];
    char second_buf[20];

    lcd.Clear(LCD_COLOR_BLACK);
    lcd.SetTextColor(LCD_COLOR_RED);
    lcd.SetBackColor(LCD_COLOR_BLACK);

    sprintf(distance_str, "%f", distance);

    char str[] = " meters.";
    strcat(distance_str, str);

    float speed = distance / 20.0f;
    sprintf(second_buf, "%f", speed);

    lcd.DisplayStringAtLine(0, (uint8_t *)"Total Distance: ");
    lcd.DisplayStringAtLine(1, (uint8_t *)distance_str);
    lcd.DisplayStringAtLine(2, (uint8_t *)"Speed (in m/s): ");
    lcd.DisplayStringAtLine(3, (uint8_t *)second_buf);
}
