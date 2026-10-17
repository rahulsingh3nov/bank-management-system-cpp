#include "Account.h"

#include <cctype>
#include <cstdio>
#include <random>
#include <sstream>
#include <vector>

#include "sha256.h"

// ---------- money helpers ----------

std::string formatMoney(Money paise) {
    const bool neg = paise < 0;
    const unsigned long long v = neg ? -static_cast<unsigned long long>(paise)
                                     : static_cast<unsigned long long>(paise);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%s%llu.%02llu", neg ? "-" : "", v / 100, v % 100);
    return buf;
}

// Accepts "1500", "1500.5", "1500.50". Rejects negatives, junk, >2 decimals.
bool parseMoney(const std::string& text, Money& out) {
    if (text.empty() || text.size() > 15) return false;
    long long whole = 0, frac = 0;
    int fracDigits = 0;
    bool seenDot = false, seenDigit = false;
    for (char c : text) {
        if (c == '.') {
            if (seenDot) return false;
            seenDot = true;
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            seenDigit = true;
            if (!seenDot) {
                whole = whole * 10 + (c - '0');
            } else {
                if (++fracDigits > 2) return false;
                frac = frac * 10 + (c - '0');
            }
        } else {
            return false;
        }
    }
    if (!seenDigit) return false;
    if (fracDigits == 1) frac *= 10;
    out = whole * 100 + frac;
    return true;
}

// ---------- Account ----------

Account::Account(int number, std::string holder, std::string salt, std::string pinHash,
                 Money balance, int failedAttempts)
    : number_(number),
      holder_(std::move(holder)),
      salt_(std::move(salt)),
      pinHash_(std::move(pinHash)),
      balance_(balance),
      failedAttempts_(failedAttempts) {}

std::string Account::makeSalt() {
    static std::random_device rd;
    static const char* hex = "0123456789abcdef";
    std::string s;
    for (int i = 0; i < 16; ++i) {
        unsigned v = rd() & 0xFF;
        s += hex[v >> 4];
        s += hex[v & 0xF];
    }
    return s;
}

std::string Account::hashPin(const std::string& pin, const std::string& salt) {
    return sha256::hash(salt + ":" + pin);
}

bool Account::validPin(const std::string& pin) {
    if (pin.size() != 4) return false;
    for (char c : pin)
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    return true;
}

bool Account::deposit(Money amount, std::string& err) {
    if (amount <= 0) {
        err = "Amount must be greater than zero.";
        return false;
    }
    balance_ += amount;
    return true;
}

bool Account::withdraw(Money amount, std::string& err) {
    if (amount <= 0) {
        err = "Amount must be greater than zero.";
        return false;
    }
    if (amount > withdrawLimit()) {
        err = "Insufficient funds. You can withdraw at most Rs. " +
              formatMoney(withdrawLimit()) + " from this " + type() + " account.";
        return false;
    }
    balance_ -= amount;
    return true;
}

bool Account::verifyPin(const std::string& pin) {
    if (locked()) return false;
    const std::string candidate = hashPin(pin, salt_);
    // Compare every character so timing does not leak where hashes differ.
    unsigned diff = candidate.size() ^ pinHash_.size();
    for (size_t i = 0; i < candidate.size() && i < pinHash_.size(); ++i)
        diff |= static_cast<unsigned>(candidate[i] ^ pinHash_[i]);
    if (diff == 0) {
        failedAttempts_ = 0;
        return true;
    }
    ++failedAttempts_;
    return false;
}

bool Account::changePin(const std::string& oldPin, const std::string& newPin, std::string& err) {
    if (!validPin(newPin)) {
        err = "New PIN must be exactly 4 digits.";
        return false;
    }
    if (!verifyPin(oldPin)) {
        err = locked() ? "Account locked." : "Current PIN is incorrect.";
        return false;
    }
    salt_ = makeSalt();
    pinHash_ = hashPin(newPin, salt_);
    return true;
}

// Record format (one per line):  type|number|holder|salt|pinHash|balancePaise|failedAttempts
std::string Account::serialize() const {
    std::ostringstream o;
    o << type() << '|' << number_ << '|' << holder_ << '|' << salt_ << '|' << pinHash_ << '|'
      << balance_ << '|' << failedAttempts_;
    return o.str();
}

std::unique_ptr<Account> Account::deserialize(const std::string& line) {
    std::vector<std::string> f;
    std::stringstream ss(line);
    std::string part;
    while (std::getline(ss, part, '|')) f.push_back(part);
    if (f.size() != 7) return nullptr;
    try {
        const int no = std::stoi(f[1]);
        const Money bal = std::stoll(f[5]);
        const int failed = std::stoi(f[6]);
        if (f[0] == "Savings")
            return std::make_unique<SavingsAccount>(no, f[2], f[3], f[4], bal, failed);
        if (f[0] == "Current")
            return std::make_unique<CurrentAccount>(no, f[2], f[3], f[4], bal, failed);
    } catch (...) {
    }
    return nullptr;
}

std::unique_ptr<Account> Account::create(char kind, int number, const std::string& holder,
                                         const std::string& pin, Money openingDeposit,
                                         std::string& err) {
    if (holder.empty() || holder.find('|') != std::string::npos) {
        err = "Name must not be empty or contain '|'.";
        return nullptr;
    }
    if (!validPin(pin)) {
        err = "PIN must be exactly 4 digits.";
        return nullptr;
    }
    if (openingDeposit < 0) {
        err = "Opening deposit cannot be negative.";
        return nullptr;
    }
    const std::string salt = makeSalt();
    const std::string hash = hashPin(pin, salt);
    if (kind == 's' || kind == 'S') {
        if (openingDeposit < SavingsAccount::MIN_BALANCE) {
            err = "Savings accounts need an opening deposit of at least Rs. " +
                  formatMoney(SavingsAccount::MIN_BALANCE) + ".";
            return nullptr;
        }
        return std::make_unique<SavingsAccount>(number, holder, salt, hash, openingDeposit, 0);
    }
    if (kind == 'c' || kind == 'C')
        return std::make_unique<CurrentAccount>(number, holder, salt, hash, openingDeposit, 0);
    err = "Unknown account type.";
    return nullptr;
}
