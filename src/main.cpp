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
float angular_velocity[40];
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
    int seconds = 20;

    while (1)
    {
        flags.wait_all(SPI_FLAG);

        // Update LCD screen to notify of seconds remaining.
        if (count % 2 == 0)
        {
            char seconds_str[20];
            sprintf(seconds_str, "%d", seconds);
            char seconds_remain[] = " seconds remaining.";
            char second_remain[] = " second remaining.";
            if (count != 38)
            {
                strcat(seconds_str, seconds_remain);
            }
            else
            {
                strcat(seconds_str, second_remain);
            }
            lcd.Clear(LCD_COLOR_BLACK);
            lcd.SetTextColor(LCD_COLOR_RED);
            lcd.SetBackColor(LCD_COLOR_BLACK);
            lcd.DisplayStringAtLine(0, (uint8_t *)"Recording...");
            lcd.DisplayStringAtLine(1, (uint8_t *)seconds_str);
            seconds -= 1;
        }

        // Break out of recording data once 20 seconds have passed.
        if (count == 40)
        {
            break;
        }

        readData();
        raw_gz = (((uint16_t)read_buf[6]) << 8) | ((uint16_t)read_buf[5]);
        gz = ((float)raw_gz) * SCALING_FACTOR;
        data_buf[count] = raw_gz;
        angular_velocity[count] = gz;
        count += 1;
    }

    // Convert angular velocity into linear velocity.
    float distance = 0.0f;
    for (int i = 0; i < 40; i += 1)
    {
        if (abs(data_buf[i]) > 500)
        {
            float one_leg_distance = (0.5f) * (abs(angular_velocity[i]) * RADIUS);
            // Account for the 2nd leg stride (the one without the sensor).
            distance += (2.0f * one_leg_distance);
        }
    }

    // Print results to LCD screen.
    char distance_str[20];
    char speed_str[20];

    lcd.Clear(LCD_COLOR_BLACK);
    lcd.SetTextColor(LCD_COLOR_RED);
    lcd.SetBackColor(LCD_COLOR_BLACK);

    sprintf(distance_str, "%f", distance);

    char meters_str[] = " meters";
    strcat(distance_str, meters_str);

    float speed = distance / 20.0f;
    sprintf(speed_str, "%f", speed);

    char metersPerSecond[] = " meters/sec";
    strcat(speed_str, metersPerSecond);

    lcd.DisplayStringAtLine(0, (uint8_t *)"Total Distance: ");
    lcd.DisplayStringAtLine(1, (uint8_t *)distance_str);
    lcd.DisplayStringAtLine(2, (uint8_t *)"Speed:");
    lcd.DisplayStringAtLine(3, (uint8_t *)speed_str);
}
