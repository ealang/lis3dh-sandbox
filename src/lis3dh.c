#include <stdint.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include "lis3dh.h"
#include "pabort.h"
#include "lis3dh-registers.h"

#define IDENTITY  0x33
#define READ_OP   0x80
#define READ_AUTO_INC_OP   (READ_OP | 0x40)
#define MAX_ACCEL_VAL_HIGHRES  (((1 << 15) - 1) & ~0xF)

// hard-coded scale setting
#define SCALE  2
#define CTRL_REG1_SCALE_FLAG  0  // ±2g

static void transaction(int fd, uint8_t *tx, uint32_t size, uint8_t *rx) {
  struct spi_ioc_transfer tr = {
    .tx_buf = (unsigned long)tx,
    .rx_buf = (unsigned long)rx,
    .len = size,
    .delay_usecs = 0,
    .speed_hz = 0,
    .bits_per_word = 8,
    .cs_change = 0
  };

  int ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
  if (ret < 1)
    pabort("can't send spi message");
}

static void write_reg(int fd, uint8_t reg, uint8_t value) {
  uint8_t rx[2];
  uint8_t tx[2] = {reg, value};
  transaction(fd, tx, 2, rx);
}

static uint8_t read_reg_8(int fd, uint8_t reg) {
  uint8_t rx[2];
  uint8_t tx[2] = {
    reg | READ_OP, 0
  };
  transaction(fd, tx, 2, rx);
  return rx[1];
}

static uint16_t read_reg_16(int fd, uint8_t reg) {
  uint8_t rx[3] = {0, 0, 0};
  uint8_t tx[3] = {
    reg | READ_AUTO_INC_OP, 0, 0
  };
  transaction(fd, tx, 3, rx);
  return (uint16_t)rx[1] | (uint16_t)rx[2] << 8;
}

uint8_t lis3dh_initialize(int fd, uint8_t sample_rate_flags) {
  uint8_t axes_en_flags = 0x7; // enable all axes
  uint8_t reg_1_val = sample_rate_flags << 4 | axes_en_flags;
  write_reg(fd, LIS3DH_CTRL_REG1, reg_1_val);

  uint8_t scale_flag = CTRL_REG1_SCALE_FLAG;
  uint8_t high_res_flag = 1; // enable high res
  uint8_t reg_4_val = scale_flag << 4 | high_res_flag << 3;
  write_reg(fd, LIS3DH_CTRL_REG4, reg_4_val);
}

void lis3dh_self_check(int fd) {
  if (read_reg_8(fd, LIS3DH_WHO_AM_I) != IDENTITY)
    pabort("failed to communicate with lis3dh");
}

struct Lis3dhStatus lis3dh_status(int fd) {
  uint8_t reg = read_reg_8(fd, LIS3DH_STATUS_REG2);
  struct Lis3dhStatus value = {
    .overrun = reg & 0x80,
    .data_available = reg & 0x8
  };
  return value;
}

static float accel_to_float(int16_t accel) {
  return (float)accel / MAX_ACCEL_VAL_HIGHRES * SCALE;
}

static float accel_from_reg(int fd, uint8_t reg) {
  return accel_to_float((int16_t)read_reg_16(fd, reg));
}

struct Accel3 lis3dh_sample_accel(int fd) {
  struct Accel3 value = {
    .x = accel_from_reg(fd, LIS3DH_OUT_X_L),
    .y = accel_from_reg(fd, LIS3DH_OUT_Y_L),
    .z = accel_from_reg(fd, LIS3DH_OUT_Z_L)
  };
  return value;
}
