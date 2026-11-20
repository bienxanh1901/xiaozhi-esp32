#pragma once

#include <string>
#include <vector>

namespace apps {

/**
 * The rules of a simple calculator for children: whole and decimal numbers,
 * + - x :, with x and : done before + and -, as taught at school.
 *
 * Keys: '0'-'9', '.', '+', '-', 'x', ':', '=', 'C' (clear) and 'B' (delete
 * the last character). Unknown keys are ignored. No LVGL here, so it is
 * tested on the host (scripts/tests/calculator_test.cc).
 */
class Calculator {
public:
    void Press(char key);

    // What has been typed, e.g. "12 + 3 x 4", or the result after '='.
    std::string Main() const;
    // The line above it: the answer so far ("= 24") while typing, or after
    // '=' the sum that was worked out ("12 + 3 x 4 =").
    std::string Hint() const;
    // True after a sum that cannot be worked out (dividing by zero, too big).
    bool HasError() const { return !error_.empty(); }

    // Formats a number the way the display shows it: no trailing zeros, at
    // most 10 significant digits.
    static std::string Format(double value);

private:
    static constexpr size_t kMaxDigits = 10;  // Per number
    static constexpr size_t kMaxTokens = 15;  // 8 numbers and 7 signs

    std::vector<std::string> tokens_;  // Numbers and signs, alternating
    std::string done_;                 // The sum shown after '='
    std::string error_;

    bool LastIsNumber() const;
    // Works out the tokens; false (with error_ set) when it cannot.
    bool Evaluate(double& result, std::string& error) const;
    static std::string Join(const std::vector<std::string>& tokens);
};

}  // namespace apps
