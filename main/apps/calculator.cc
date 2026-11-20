#include "calculator.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace apps {

namespace {

constexpr double kMaxMagnitude = 1e10;  // Beyond this the answer does not fit

bool IsSign(const std::string& token) {
    return token == "+" || token == "-" || token == "x" || token == ":";
}

size_t DigitCount(const std::string& number) {
    size_t digits = 0;
    for (char c : number) {
        if (c >= '0' && c <= '9')
            ++digits;
    }
    return digits;
}

}  // namespace

std::string Calculator::Format(double value) {
    if (value == 0)
        return "0";  // Also turns -0 into 0
    char text[32];
    snprintf(text, sizeof(text), "%.10g", value);
    std::string result = text;
    // %g switches to an exponent for tiny values; round those to plain digits.
    if (result.find('e') != std::string::npos) {
        snprintf(text, sizeof(text), "%.8f", value);
        result = text;
        while (result.back() == '0')
            result.pop_back();
        if (result.back() == '.')
            result.pop_back();
        if (result == "-0" || result.empty())
            result = "0";
    }
    return result;
}

bool Calculator::LastIsNumber() const { return !tokens_.empty() && !IsSign(tokens_.back()); }

std::string Calculator::Join(const std::vector<std::string>& tokens) {
    std::string text;
    for (const auto& token : tokens) {
        if (!text.empty())
            text += ' ';
        text += token;
    }
    return text;
}

void Calculator::Press(char key) {
    if (key == 'C') {
        tokens_.clear();
        done_.clear();
        error_.clear();
        return;
    }
    if (!error_.empty()) {
        // Any key starts again after an error; a digit also starts the new sum.
        tokens_.clear();
        done_.clear();
        error_.clear();
        if (key == 'B' || key == '=' || IsSign(std::string(1, key)))
            return;
    }
    const bool after_equals = !done_.empty();

    if ((key >= '0' && key <= '9') || key == '.') {
        if (after_equals) {
            tokens_.clear();  // A new number starts a new sum
            done_.clear();
        }
        if (!LastIsNumber()) {
            if (tokens_.size() >= kMaxTokens)
                return;
            tokens_.push_back(key == '.' ? "0." : std::string(1, key));
            return;
        }
        std::string& number = tokens_.back();
        if (key == '.') {
            if (number.find('.') == std::string::npos)
                number += '.';
            return;
        }
        if (DigitCount(number) >= kMaxDigits)
            return;
        if (number == "0") {
            number = key;  // No leading zeros
        } else if (number == "-0") {
            number = std::string("-") + key;
        } else {
            number += key;
        }
        return;
    }

    if (IsSign(std::string(1, key))) {
        done_.clear();  // After '=', carry on from the answer
        if (tokens_.empty()) {
            if (key != '-')
                return;
            tokens_.push_back("0");  // "- 5" means 0 - 5
        }
        if (!LastIsNumber()) {
            tokens_.back() = std::string(1, key);  // Change the sign
            return;
        }
        if (tokens_.size() >= kMaxTokens - 1)
            return;
        tokens_.push_back(std::string(1, key));
        return;
    }

    if (key == 'B') {
        if (after_equals) {
            tokens_.clear();
            done_.clear();
            return;
        }
        if (tokens_.empty())
            return;
        std::string& last = tokens_.back();
        last.pop_back();
        if (last.empty() || last == "-")
            tokens_.pop_back();
        return;
    }

    if (key == '=') {
        if (after_equals || tokens_.empty())
            return;
        if (!LastIsNumber())
            tokens_.pop_back();  // "5 +" means 5
        if (tokens_.size() == 1)
            return;  // Nothing to work out
        double result = 0;
        std::string error;
        std::string sum = Join(tokens_) + " =";
        if (!Evaluate(result, error)) {
            error_ = error;
            done_ = sum;
            return;
        }
        done_ = sum;
        tokens_ = {Format(result)};
    }
}

bool Calculator::Evaluate(double& result, std::string& error) const {
    // First x and :, left to right, collecting the terms to add.
    std::vector<double> terms;
    std::vector<char> signs;  // '+' or '-' before each term after the first
    double term = strtod(tokens_[0].c_str(), nullptr);
    for (size_t i = 1; i + 1 < tokens_.size(); i += 2) {
        const char sign = tokens_[i][0];
        const double value = strtod(tokens_[i + 1].c_str(), nullptr);
        if (sign == 'x') {
            term *= value;
        } else if (sign == ':') {
            if (value == 0) {
                error = "Cannot divide by 0";
                return false;
            }
            term /= value;
        } else {
            terms.push_back(term);
            signs.push_back(sign);
            term = value;
        }
        if (std::fabs(term) >= kMaxMagnitude || !std::isfinite(term)) {
            error = "Too big";
            return false;
        }
    }
    terms.push_back(term);

    result = terms[0];
    for (size_t i = 0; i < signs.size(); ++i) {
        result = signs[i] == '+' ? result + terms[i + 1] : result - terms[i + 1];
    }
    if (std::fabs(result) >= kMaxMagnitude || !std::isfinite(result)) {
        error = "Too big";
        return false;
    }
    return true;
}

std::string Calculator::Main() const {
    if (!error_.empty())
        return error_;
    return tokens_.empty() ? "0" : Join(tokens_);
}

std::string Calculator::Hint() const {
    if (!done_.empty())
        return done_;
    // While typing, show the answer so far once there is something to work out.
    std::vector<std::string> tokens = tokens_;
    if (!tokens.empty() && IsSign(tokens.back()))
        tokens.pop_back();
    if (tokens.size() < 3)
        return "";
    Calculator preview;
    preview.tokens_ = tokens;
    double result = 0;
    std::string error;
    if (!preview.Evaluate(result, error))
        return "";
    return "= " + Format(result);
}

}  // namespace apps
