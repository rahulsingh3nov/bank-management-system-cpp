#pragma once
#include <map>
#include <memory>
#include <string>

#include "Account.h"

// Owns all accounts and handles loading/saving them to disk.
class Bank {
public:
    explicit Bank(std::string dataFile);

    // Reads the data file (a missing file simply means "no accounts yet").
    // Returns the number of corrupt lines skipped.
    int load();
    // Writes to a temp file first, then renames it, so a crash mid-write
    // can never leave a half-written database behind.
    bool save() const;

    Account* openAccount(char kind, const std::string& holder, const std::string& pin,
                         Money openingDeposit, std::string& err);
    Account* find(int number);
    size_t count() const { return accounts_.size(); }

private:
    std::string dataFile_;
    std::map<int, std::unique_ptr<Account>> accounts_;
    int nextNumber_ = 1001;
};
