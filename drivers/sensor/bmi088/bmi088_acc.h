/* Bosch BMI088 inertial measurement unit header
 * Note: This is for the Accelerometer part only
 *
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_BMI088_BMI088_H_
#define ZEPHYR_DRIVERS_SENSOR_BMI088_BMI088_H_

#include <drivers/sensor.h>
#include <drivers/spi.h>
#include <sys/util.h>

// acc register

// read-only
#define BMI088_REG_CHIPID    0x00
#define RATE_X_LSB      0x12
#define BMI088_ACC_STATUS   0x03

// write-only
#define BMI088_SOFTRESET  0x7E

// read/write
#define ACC_RANGE      0x41
#define ACC_PWR_CTRL    0x7D

//other defines


#define BMI088_REG_READ BIT(7)  // Indicates a read operation; bit 7 is clear on write s
#define BMI088_REG_MASK 0x7f // Mask lower 7 bits for register addresses
#define BMI088_STATUS_MASK BIT(7) // Bit 7 is the

#define BMI088_CHIP_ID 0x1E  // Reset value of BMI088_REG_CHIPID

#define BMI088_SR_VAL   0xB6    // Value for triggering a Soft-Reset

#define BMI088_DEFAULT_RANGE    0x03    // Largest possible range for acc (+- 24g)

#define ACC_NORMAL_MODE     0x04    // Value for switching to normal mode

#define BMI088_AXES 3   // Number of Axes

#define BMI088_SAMPLE_SIZE  (BMI088_AXES * sizeof(uint16_t))    // Size of Samples with 2 bytes per axis = 6 bytes

// end of default settings

struct bmi088_cfg {
    struct spi_dt_spec bus;
};

struct bmi088_range {
    uint16_t range;
    uint8_t reg_val;
};

// Each sample has X, Y and Z, each with lsb and msb Bytes
struct bmi088_acc_sample {
    uint16_t acc[BMI088_AXES];
};

struct bmi088_data {
    const struct device *bus;
    struct bmi088_acc_sample sample;
};

static inline struct bmi088_data *to_data(const struct device *dev) {
    return dev->data;
}

static inline const struct bmi088_cfg *to_config(const struct device *dev) {
    return dev->config;
}

/**
 * Read multiple bytes from the BMI088
 *
 * @param dev Sensor device pointer
 * @param reg_addr Register address
 * @param [out] data Destination address
 * @param len Number of bytes to read
 * @return 0 on success
 */
int bmi088_read(const struct device *dev, uint8_t reg_addr, void *data, uint8_t len);

int bmi088_byte_read(const struct device *dev, uint8_t reg_addr, uint8_t *byte);

/**
 * Write multiple bytes to the BMI088
 *
 * @param dev Sensor device pointer
 * @param reg_addr Register address
 * @param [out] buf Data to write to the BMI088
 * @param len Number of bytes to write
 * @return 0 on success
 */
int bmi088_write(const struct device *dev, uint8_t reg_addr, void *buf, uint8_t len);

int bmi088_byte_write(const struct device *dev, uint8_t reg_addr, uint8_t byte);

/**
 * Update some bits in a BMI088 register without changing the other bits.
 * Pos and Mask are rather redundant, but make calculation easier.
 * It does not make sense to have a mask that is not a contiguous sequence of ones, offset by pos.
 *
 * @param dev Sensor device pointer
 * @param reg_addr Register address
 * @param pos Offset from the least-significant bit for the value
 * @param mask Mask that specifies which bits are allowed to change
 * @param val Value to set the bits to
 * @return
 */
int bmi088_reg_field_update(const struct device *dev, uint8_t reg_addr, uint8_t pos, uint8_t mask, uint8_t val);


struct sensor_value bmi088_to_fixed_point(int16_t raw_val, uint16_t scale);

struct sensor_value bmi088_channel_convert(enum sensor_channel chan, uint16_t scale, int16_t raw_xyz[3]);

#endif /* ZEPHYR_DRIVERS_SENSOR_BMI088_BMI088_H_ */
