#include <cctype>
#include <iostream>
#include <string>

#include "Bank.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace {

bool readLine(const std::string& prompt, std::string& out) {
    std::cout << prompt << std::flush;
    if (!std::getline(std::cin, out)) return false;
    // trim
    size_t b = out.find_first_not_of(" \t\r"), e = out.find_last_not_of(" \t\r");
    out = (b == std::string::npos) ? "" : out.substr(b, e - b + 1);
    return true;
}

// Reads a PIN without echoing it to the terminal (falls back to normal input when piped).
bool readPin(const std::string& prompt, std::string& out) {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    const bool tty = GetConsoleMode(h, &mode) != 0;
    if (tty) SetConsoleMode(h, mode & ~ENABLE_ECHO_INPUT);
    bool ok = readLine(prompt, out);
    if (tty) { SetConsoleMode(h, mode); std::cout << '\n'; }
    return ok;
#else
    termios oldt{};
    const bool tty = isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &oldt) == 0;
    if (tty) {
        termios newt = oldt;
        newt.c_lflag &= ~static_cast<tcflag_t>(ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    }
    bool ok = readLine(prompt, out);
    if (tty) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        std::cout << '\n';
    }
    return ok;
#endif
}

bool readMoney(const std::string& prompt, Money& out) {
    std::string s;
    if (!readLine(prompt, s)) return false;
    if (!parseMoney(s, out)) {
        std::cout << "  Invalid amount. Use numbers like 1500 or 1500.50.\n";
        out = -1;
    }
    return true;
}

void createAccount(Bank& bank) {
    std::string name, kind, pin, pin2;
    Money deposit = -1;
    std::cout << "\n--- Open a new account ---\n";
    if (!readLine("Full name: ", name)) return;
    if (!readLine("Account type ([S]avings / [C]urrent): ", kind)) return;
    if (!readMoney("Opening deposit (Rs.): ", deposit) || deposit < 0) return;
    if (!readPin("Choose a 4-digit PIN: ", pin)) return;
    if (!readPin("Confirm PIN: ", pin2)) return;
    if (pin != pin2) {
        std::cout << "  PINs do not match. Account not created.\n";
        return;
    }
    std::string err;
    Account* acc = bank.openAccount(kind.empty() ? ' ' : kind[0], name, pin, deposit, err);
    if (!acc) {
        std::cout << "  Could not open account: " << err << "\n";
        return;
    }
    if (!bank.save()) std::cout << "  WARNING: could not write to the data file!\n";
    std::cout << "  Account created. Your account number is " << acc->number()
              << " (keep it safe).\n";
}

void accountMenu(Bank& bank, Account& acc) {
    std::string choice;
    while (true) {
        std::cout << "\n--- " << acc.type() << " account " << acc.number() << " | "
                  << acc.holder() << " ---\n"
                  << " 1. Deposit\n 2. Withdraw\n 3. Check balance\n 4. Change PIN\n"
                  << " 0. Log out\n";
        if (!readLine("Choice: ", choice) || choice == "0") return;
        std::string err;
        Money amt = -1;
        if (choice == "1") {
            if (!readMoney("Amount to deposit (Rs.): ", amt)) return;
            if (amt < 0) continue;
            if (acc.deposit(amt, err))
                std::cout << "  Deposited Rs. " << formatMoney(amt) << ". New balance: Rs. "
                          << formatMoney(acc.balance()) << "\n";
            else
                std::cout << "  " << err << "\n";
        } else if (choice == "2") {
            if (!readMoney("Amount to withdraw (Rs.): ", amt)) return;
            if (amt < 0) continue;
            if (acc.withdraw(amt, err))
                std::cout << "  Withdrew Rs. " << formatMoney(amt) << ". New balance: Rs. "
                          << formatMoney(acc.balance()) << "\n";
            else
                std::cout << "  " << err << "\n";
        } else if (choice == "3") {
            std::cout << "  Current balance: Rs. " << formatMoney(acc.balance()) << "\n";
            continue;
        } else if (choice == "4") {
            std::string oldPin, newPin;
            if (!readPin("Current PIN: ", oldPin) || !readPin("New 4-digit PIN: ", newPin)) return;
            if (acc.changePin(oldPin, newPin, err))
                std::cout << "  PIN changed.\n";
            else
                std::cout << "  " << err << "\n";
            if (acc.locked()) {
                bank.save();
                return;
            }
        } else {
            std::cout << "  Please pick 0-4.\n";
            continue;
        }
        if (!bank.save()) std::cout << "  WARNING: could not write to the data file!\n";
    }
}

void login(Bank& bank) {
    std::string numStr, pin;
    std::cout << "\n--- Log in ---\n";
    if (!readLine("Account number: ", numStr)) return;
    if (!readPin("PIN: ", pin)) return;

    int number = 0;
    try { number = std::stoi(numStr); } catch (...) {}
    Account* acc = bank.find(number);
    // Same message for unknown account and wrong PIN: don't reveal which accounts exist.
    if (!acc) {
        std::cout << "  Invalid account number or PIN.\n";
        return;
    }
    if (acc->locked()) {
        std::cout << "  This account is locked after too many wrong PINs. Contact the bank.\n";
        return;
    }
    const bool ok = acc->verifyPin(pin);
    bank.save();  // persist the attempt counter either way
    if (!ok) {
        if (acc->locked())
            std::cout << "  Invalid PIN. Account is now LOCKED.\n";
        else
            std::cout << "  Invalid account number or PIN. (" << acc->attemptsLeft()
                      << " attempt(s) left)\n";
        return;
    }
    accountMenu(bank, *acc);
}

}  // namespace

int main(int argc, char** argv) {
    const std::string dataFile = argc > 1 ? argv[1] : "data/bank.dat";
    Bank bank(dataFile);
    const int skipped = bank.load();
    if (skipped > 0) std::cout << "Note: skipped " << skipped << " unreadable record(s).\n";

    std::cout << "=========================================\n"
              << "        BANK MANAGEMENT SYSTEM (C++)\n"
              << "=========================================\n";
    std::string choice;
    while (true) {
        std::cout << "\n 1. Open new account\n 2. Log in\n 0. Exit\n";
        if (!readLine("Choice: ", choice) || choice == "0") break;
        if (choice == "1") createAccount(bank);
        else if (choice == "2") login(bank);
        else std::cout << "  Please pick 0-2.\n";
    }
    std::cout << "Thank you for banking with us. Goodbye!\n";
    return 0;
}
