/// @file test_fpu.c
/// @brief FPU unit tests.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUNIT ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "devices/fpu.h"
#include "math.h"
#include "tests/test.h"
#include "tests/test_utils.h"

/// @brief Check if two floating point numbers are equal within a tolerance.
/// @param a First value.
/// @param b Second value.
/// @param epsilon Tolerance for comparison.
/// @return 1 if |a - b| <= epsilon, 0 otherwise.
static inline int check_float_equality(double a, double b, double epsilon)
{
    return (fabs(a - b) <= epsilon) ? 1 : 0;
}

/// @brief Test that FPU is initialized.
TEST(fpu_initialized)
{
    TEST_SECTION_START("FPU initialization check");
    // Check that FPU is properly initialized after boot
    ASSERT(fpu_is_initialized() == 1);
    TEST_SECTION_END();
}

/// @brief Test basic floating point addition.
TEST(fpu_addition)
{
    TEST_SECTION_START("FPU addition");
    double a      = 3.14;
    double b      = 2.86;
    double result = a + b;
    ASSERT(check_float_equality(result, 6.0, macheps(6.0)));
    TEST_SECTION_END();
}

/// @brief Test basic floating point multiplication.
TEST(fpu_multiplication)
{
    TEST_SECTION_START("FPU multiplication");
    double a      = 3.0;
    double b      = 4.0;
    double result = a * b;
    ASSERT(check_float_equality(result, 12.0, macheps(12.0)));
    TEST_SECTION_END();
}

/// @brief Test floating point division.
TEST(fpu_division)
{
    TEST_SECTION_START("FPU division");
    double a      = 10.0;
    double b      = 2.0;
    double result = a / b;
    ASSERT(check_float_equality(result, 5.0, macheps(5.0)));
    TEST_SECTION_END();
}

/// @brief Test floating point square root.
TEST(fpu_sqrt)
{
    TEST_SECTION_START("FPU square root");
    double a      = 9.0;
    double result = sqrt(a);
    ASSERT(check_float_equality(result, 3.0, macheps(3.0)));
    TEST_SECTION_END();
}

/// @brief Test floating point sine function.
TEST(fpu_sin)
{
    TEST_SECTION_START("FPU sine");
    double a      = 0.0;
    double result = sin(a);
    ASSERT(check_float_equality(result, 0.0, macheps(0.0)));
    TEST_SECTION_END();
}

/// @brief Test floating point cosine function.
TEST(fpu_cos)
{
    TEST_SECTION_START("FPU cosine");
    double a      = 0.0;
    double result = cos(a);
    ASSERT(check_float_equality(result, 1.0, macheps(1.0)));
    TEST_SECTION_END();
}

/// @brief Test floating point power function.
TEST(fpu_pow)
{
    TEST_SECTION_START("FPU power");
    double a      = 2.0;
    double b      = 3.0;
    double result = pow(a, b);
    ASSERT(check_float_equality(result, 8.0, macheps(8.0)));
    TEST_SECTION_END();
}

/// @brief Test floating point logarithm.
TEST(fpu_log)
{
    TEST_SECTION_START("FPU logarithm");
    double a      = 1.0;
    double result = log(a);
    ASSERT(check_float_equality(result, 0.0, macheps(0.0)));
    TEST_SECTION_END();
}

/// @brief Test floating point exponential.
TEST(fpu_exp)
{
    TEST_SECTION_START("FPU exponential");
    double a      = 0.0;
    double result = exp(a);
    ASSERT(check_float_equality(result, 1.0, macheps(1.0)));
    TEST_SECTION_END();
}

/// @brief Test floating point precision with PI.
TEST(fpu_pi_precision)
{
    TEST_SECTION_START("FPU PI precision");
    double pi = M_PI;
    // Check that PI is approximately 3.14159
    ASSERT(pi > 3.141 && pi < 3.142);
    TEST_SECTION_END();
}

/// @brief Test floating point loop accumulation.
TEST(fpu_loop_accumulation)
{
    TEST_SECTION_START("FPU loop accumulation");
    double sum = 0.0;
    for (int i = 1; i <= 10; i++) {
        sum += 1.0 / i;
    }
    // Harmonic series H_10 ≈ 2.9289682539682538
    ASSERT(sum > 2.9 && sum < 2.95);
    TEST_SECTION_END();
}

/// @brief Run all FPU tests.
void test_fpu(void)
{
    test_fpu_initialized();
    test_fpu_addition();
    test_fpu_multiplication();
    test_fpu_division();
    test_fpu_sqrt();
    test_fpu_sin();
    test_fpu_cos();
    test_fpu_pow();
    test_fpu_log();
    test_fpu_exp();
    test_fpu_pi_precision();
    test_fpu_loop_accumulation();
}