#ifndef KERNEL_TEST_H
#define KERNEL_TEST_H

#include <drivers/serial.h>

extern int kernel_test_failures;

#define TEST_START(name) \
    serial_write(SERIAL_COM1_BASE, "[TEST_START] " name "\n")

#define TEST_PASS(name, msg) \
    serial_write(SERIAL_COM1_BASE, "[TEST_PASS] " name ": " msg "\n")

#define TEST_FAIL(name, msg)                                               \
    do                                                                     \
    {                                                                      \
        kernel_test_failures++;                                            \
        serial_write(SERIAL_COM1_BASE, "[TEST_FAIL] " name ": " msg "\n"); \
    } while (0)

#define TEST_END() \
    serial_write(SERIAL_COM1_BASE, "[TEST_END]\n")

#endif /* KERNEL_TEST_H */
