// Host test for main/apps/calculator.cc; built and run by test_calculator.py.
#include <cstdio>
#include <string>

#include "calculator.h"

using apps::Calculator;

static int failures = 0;
#define CHECK_EQ(actual, expected)                                                       \
    do {                                                                                 \
        const std::string a = (actual), e = (expected);                                  \
        if (a != e) {                                                                    \
            printf("FAILED line %d: %s is \"%s\", expected \"%s\"\n", __LINE__, #actual, \
                   a.c_str(), e.c_str());                                                \
            ++failures;                                                                  \
        }                                                                                \
    } while (0)

static Calculator Type(const char* keys) {
    Calculator calc;
    for (const char* k = keys; *k; ++k)
        calc.Press(*k);
    return calc;
}

int main() {
    CHECK_EQ(Type("").Main(), "0");
    CHECK_EQ(Type("12+7").Main(), "12 + 7");
    CHECK_EQ(Type("12+7").Hint(), "= 19");
    CHECK_EQ(Type("12+7=").Main(), "19");
    CHECK_EQ(Type("12+7=").Hint(), "12 + 7 =");

    // x and : before + and -.
    CHECK_EQ(Type("2+3x4=").Main(), "14");
    CHECK_EQ(Type("20-6:3=").Main(), "18");
    CHECK_EQ(Type("10-2-3=").Main(), "5");
    CHECK_EQ(Type("8:2:2=").Main(), "2");
    CHECK_EQ(Type("3-5=").Main(), "-2");

    // Decimals and formatting.
    CHECK_EQ(Type("1:3=").Main(), "0.3333333333");
    CHECK_EQ(Type("0.1+0.2=").Main(), "0.3");
    CHECK_EQ(Type("2.5x4=").Main(), "10");
    CHECK_EQ(Type(".5+1=").Main(), "1.5");
    CHECK_EQ(Type("1..5").Main(), "1.5");
    CHECK_EQ(Type("5.+1=").Main(), "6");

    // Entry rules.
    CHECK_EQ(Type("007").Main(), "7");
    CHECK_EQ(Type("5+-").Main(), "5 -");   // A second sign replaces the first
    CHECK_EQ(Type("+5").Main(), "5");      // No sign before the first number
    CHECK_EQ(Type("-5").Main(), "0 - 5");  // A leading minus starts from 0
    CHECK_EQ(Type("-5=").Main(), "-5");
    CHECK_EQ(Type("-5+1=").Main(), "-4");
    CHECK_EQ(Type("5+=").Main(), "5");
    CHECK_EQ(Type("12345678901").Main(), "1234567890");
    CHECK_EQ(Type("12B").Main(), "1");
    CHECK_EQ(Type("12+B").Main(), "12");
    CHECK_EQ(Type("1B").Main(), "0");
    CHECK_EQ(Type("12+3C").Main(), "0");
    CHECK_EQ(Type("12+3C").Hint(), "");

    // After '=': a sign carries on from the answer, a digit starts again.
    CHECK_EQ(Type("2+3=x2=").Main(), "10");
    CHECK_EQ(Type("2+3=7").Main(), "7");
    CHECK_EQ(Type("2+3==").Main(), "5");
    CHECK_EQ(Type("2+3=B").Main(), "0");
    CHECK_EQ(Type("3-5=B").Main(), "0");
    CHECK_EQ(Type("3-5=x2=").Main(), "-4");

    // Errors, and starting again after one.
    Calculator calc = Type("5:0=");
    CHECK_EQ(calc.Main(), "Cannot divide by 0");
    CHECK_EQ(std::string(calc.HasError() ? "yes" : "no"), "yes");
    calc.Press('7');
    CHECK_EQ(calc.Main(), "7");
    CHECK_EQ(Type("5:0=+").Main(), "0");
    CHECK_EQ(Type("99999x99999x99=").Main(), "Too big");
    CHECK_EQ(Type("5:0").Hint(), "");

    // The sum stays a sensible length: at most 8 numbers.
    CHECK_EQ(Type("1+1+1+1+1+1+1+1+").Main(), "1 + 1 + 1 + 1 + 1 + 1 + 1 + 1");
    CHECK_EQ(Type("1+1+1+1+1+1+1+1=").Main(), "8");

    CHECK_EQ(Calculator::Format(-0.0), "0");
    CHECK_EQ(Calculator::Format(1e-9), "0");
    CHECK_EQ(Calculator::Format(0.00001234), "0.00001234");

    if (failures == 0)
        printf("All calculator tests passed\n");
    return failures == 0 ? 0 : 1;
}
