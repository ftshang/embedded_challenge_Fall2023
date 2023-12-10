#include "mbed.h"

#define CTRL_REG1 0x20
#define CTRL_REG1_CONFIG 0b01'10'1'1'1'1

#define CTRL_REG4 0x23
#define CTRL_REG4_CONFIG 0b0'0'01'0'00'0

#define OUT_X_L 0x28
#define SPI_FLAG 1

#define SCALING_FACTOR (17.5f * 0.17453292519943295769236907684886f / 1000.0f)

EventFlags flags;

void spi_cb(int event)
{
    flags.set(SPI_FLAG);
}

int main()
{
    SPI spi(PF_9, PF_8, PF_7, PC_1, use_gpio_ssel);

    uint8_t write_buf[32], read_buf[32];

    spi.format(8, 3);
    spi.frequency(1'000'000);

    write_buf[0] = CTRL_REG1;
    write_buf[1] = CTRL_REG1_CONFIG;

    spi.transfer(write_buf, 2, read_buf, 2, spi_cb);
    flags.wait_all(SPI_FLAG);

    write_buf[0] = CTRL_REG4;
    write_buf[1] = CTRL_REG4_CONFIG;

    spi.transfer(write_buf, 2, read_buf, 2, spi_cb);
    flags.wait_all(SPI_FLAG);

    write_buf[1] = 0xFF;

    while (1)
    {
        int16_t raw_gx, raw_gy, raw_gz;
        float gx, gy, gz;

        write_buf[0] = OUT_X_L | 0x80 | 0x40;

        spi.transfer(write_buf, 7, read_buf, 7, spi_cb);
        flags.wait_all(SPI_FLAG);

        raw_gx = (((uint16_t)read_buf[2]) << 8) | ((uint16_t)read_buf[1]);
        raw_gy = (((uint16_t)read_buf[4]) << 8) | ((uint16_t)read_buf[3]);
        raw_gz = (((uint16_t)read_buf[6]) << 8) | ((uint16_t)read_buf[5]);
        printf("RAW -> \t\tgx: %d \t gy: %d\t gz: %d\t\n", raw_gx, raw_gy, raw_gz);

        printf(">x_axis: %d|g\n", raw_gx);
        printf(">y_axis: %d|g\n", raw_gy);
        printf(">z_axis: %d|g\n", raw_gz);

        gx = ((float) raw_gx) * SCALING_FACTOR;
        gy = ((float) raw_gy) * SCALING_FACTOR;
        gz = ((float) raw_gz) * SCALING_FACTOR;

        printf("Actual ->\tgx: %4.5f \t gy: %4.5f \t gz: %4.5f\n\n", gx, gy, gz);
        thread_sleep_for(100);
    }
}
