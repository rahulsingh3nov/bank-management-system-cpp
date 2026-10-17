#include "Bank.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

Bank::Bank(std::string dataFile) : dataFile_(std::move(dataFile)) {}

int Bank::load() {
    std::ifstream in(dataFile_);
    if (!in) return 0;
    int skipped = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto acc = Account::deserialize(line);
        if (!acc || accounts_.count(acc->number())) {
            ++skipped;
            continue;
        }
        if (acc->number() >= nextNumber_) nextNumber_ = acc->number() + 1;
        accounts_[acc->number()] = std::move(acc);
    }
    return skipped;
}

bool Bank::save() const {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path target(dataFile_);
    if (target.has_parent_path()) fs::create_directories(target.parent_path(), ec);

    const std::string tmp = dataFile_ + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) return false;
        for (const auto& [no, acc] : accounts_) out << acc->serialize() << '\n';
        out.flush();
        if (!out) return false;
    }
    // Restrict the file to the current user where the OS supports it.
    fs::permissions(tmp, fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::replace, ec);
    fs::rename(tmp, target, ec);
    return !ec;
}

Account* Bank::openAccount(char kind, const std::string& holder, const std::string& pin,
                           Money openingDeposit, std::string& err) {
    auto acc = Account::create(kind, nextNumber_, holder, pin, openingDeposit, err);
    if (!acc) return nullptr;
    Account* raw = acc.get();
    accounts_[nextNumber_++] = std::move(acc);
    return raw;
}

Account* Bank::find(int number) {
    auto it = accounts_.find(number);
    return it == accounts_.end() ? nullptr : it->second.get();
}
