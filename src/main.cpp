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
#define LEG_LENGTH (0.95)
#define FILTER_COEFFICIENT 0.1f
#define THRESHOLD (3750)

SPI spi(PF_9, PF_8, PF_7, PC_1, use_gpio_ssel);
int8_t write_buf[32];
int8_t read_buf[32];
int16_t data_buf[40];
float linear_distance[40];
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
    int16_t raw_gx, raw_gy, raw_gz;
    float gx, gy, gz;
    // float filtered_gx, filtered_gy, filtered_gz = 0.0f;
    t.attach(&cb, 500ms);
    setupGyroscope();

    lcd.Clear(LCD_COLOR_BLACK);
    lcd.SetTextColor(LCD_COLOR_RED);
    lcd.SetBackColor(LCD_COLOR_BLACK);
    lcd.DisplayStringAtLine(0, (uint8_t *)"Begin walking now.");

    printf("start\n");

    while (1)
    {
        flags.wait_all(SPI_FLAG);

        if (count == 40)
        {
            break;
        }
        readData();
        raw_gx = (((uint16_t)read_buf[2]) << 8) | ((uint16_t)read_buf[1]);
        raw_gy = (((uint16_t)read_buf[4]) << 8) | ((uint16_t)read_buf[3]);
        raw_gz = (((uint16_t)read_buf[6]) << 8) | ((uint16_t)read_buf[5]);

        printf("RAW|\tgx: %d \t gy: %d \t gz: %d\t\n", raw_gx, raw_gy, raw_gz);

        printf(">x_axis_raw:%d|g\n", raw_gx);
        printf(">y_axis_raw:%d|g\n", raw_gy);
        printf(">z_axis_raw:%d|g\n", raw_gz);

        gx = ((float)raw_gx) * SCALING_FACTOR;
        gy = ((float)raw_gx) * SCALING_FACTOR;
        gz = ((float)raw_gz) * SCALING_FACTOR;

        printf("Actual -> \t\tgx: %4.5f \t gy: %4.5f \t gz: %4.5f\t\n", gx, gy, gz);

        // Calculate the linear distance
        float angularDistanceX = 0.5 * gx;
        float angularDistanceY = 0.5 * gy;
        float angularDistanceZ = 0.5 * gz;

        float linearDistanceX = angularDistanceX * LEG_LENGTH;
        float linearDistanceY = angularDistanceY * LEG_LENGTH;
        float linearDistanceZ = angularDistanceZ * LEG_LENGTH;

        linear_distance[count] = sqrt((linearDistanceX * linearDistanceX) + (linearDistanceY * linearDistanceY) + (linearDistanceZ * linearDistanceZ));

        data_buf[count] = raw_gz;
        count += 1;
    }

    // int numSteps = 0;
    // int16_t lastValue = data_buf[0];
    float distance = 0.0f;

    for (int i = 0; i < 40; i += 1)
    {
        // if (abs(data_buf[i]) > THRESHOLD)
        // {
        //     numSteps += 1;
        // }
        // lastValue = data_buf[i];
        // printf("Absolute value: %d\t Normal value: %d\n", abs(data_buf[i]), data_buf[i]);

        distance += linear_distance[i];
    }

    // float totalDistance = numSteps * LEG_LENGTH;

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
