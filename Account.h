#pragma once
#include <memory>
#include <string>

// All money is stored as an integer number of paise (1 rupee = 100 paise)
// so that no floating-point rounding errors can creep into balances.
using Money = long long;

std::string formatMoney(Money paise);
bool parseMoney(const std::string& text, Money& out);

// Abstract base class: encapsulates data, hashing and persistence logic.
// Derived classes supply the withdrawal rule (polymorphism).
class Account {
public:
    static constexpr int MAX_ATTEMPTS = 3;

    Account(int number, std::string holder, std::string salt, std::string pinHash,
            Money balance, int failedAttempts);
    virtual ~Account() = default;

    // --- accessors ---
    int number() const { return number_; }
    const std::string& holder() const { return holder_; }
    Money balance() const { return balance_; }
    bool locked() const { return failedAttempts_ >= MAX_ATTEMPTS; }
    int attemptsLeft() const { return MAX_ATTEMPTS - failedAttempts_; }

    // --- polymorphic behaviour ---
    virtual std::string type() const = 0;
    // Largest amount that may be withdrawn right now.
    virtual Money withdrawLimit() const = 0;

    // --- operations (return false and fill `err` on failure) ---
    bool deposit(Money amount, std::string& err);
    bool withdraw(Money amount, std::string& err);
    bool verifyPin(const std::string& pin);  // counts failures, locks after MAX_ATTEMPTS
    bool changePin(const std::string& oldPin, const std::string& newPin, std::string& err);

    // --- persistence ---
    std::string serialize() const;
    static std::unique_ptr<Account> deserialize(const std::string& line);

    // --- factory ---
    static std::unique_ptr<Account> create(char kind, int number, const std::string& holder,
                                           const std::string& pin, Money openingDeposit,
                                           std::string& err);
    static bool validPin(const std::string& pin);

protected:
    int number_;
    std::string holder_;
    std::string salt_;
    std::string pinHash_;
    Money balance_;
    int failedAttempts_;

private:
    static std::string makeSalt();
    static std::string hashPin(const std::string& pin, const std::string& salt);
};

// Savings: must always keep a minimum balance.
class SavingsAccount : public Account {
public:
    static constexpr Money MIN_BALANCE = 500 * 100;  // Rs. 500
    using Account::Account;
    std::string type() const override { return "Savings"; }
    Money withdrawLimit() const override {
        return balance_ > MIN_BALANCE ? balance_ - MIN_BALANCE : 0;
    }
};

// Current: allowed to dip below zero up to an overdraft limit.
class CurrentAccount : public Account {
public:
    static constexpr Money OVERDRAFT_LIMIT = 10000 * 100;  // Rs. 10,000
    using Account::Account;
    std::string type() const override { return "Current"; }
    Money withdrawLimit() const override { return balance_ + OVERDRAFT_LIMIT; }
};
