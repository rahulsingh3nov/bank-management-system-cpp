// Dependency-free tests. Build & run with:  make test
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "Bank.h"
#include "sha256.h"

static int failures = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::cerr << "FAIL line " << __LINE__ << ": " #cond << "\n";     \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

int main() {
    // SHA-256 known-answer tests
    CHECK(sha256::hash("abc") ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(sha256::hash("") ==
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    // Money parsing / formatting
    Money m = 0;
    CHECK(parseMoney("1500", m) && m == 150000);
    CHECK(parseMoney("12.5", m) && m == 1250);
    CHECK(parseMoney("0.07", m) && m == 7);
    CHECK(!parseMoney("-5", m));
    CHECK(!parseMoney("1.234", m));
    CHECK(!parseMoney("abc", m));
    CHECK(!parseMoney("", m));
    CHECK(formatMoney(123456) == "1234.56");
    CHECK(formatMoney(-5) == "-0.05");

    const std::string file = "test_bank_tmp.dat";
    std::filesystem::remove(file);
    std::string err;
    {
        Bank bank(file);
        CHECK(bank.load() == 0);

        // Savings rules
        CHECK(bank.openAccount('s', "Asha", "1234", 10000, err) == nullptr);  // < Rs.500
        Account* s = bank.openAccount('s', "Asha", "1234", 100000, err);
        CHECK(s && s->number() == 1001);
        CHECK(!s->withdraw(95000, err));  // would breach minimum balance
        CHECK(s->withdraw(50000, err));
        CHECK(s->balance() == 50000);
        CHECK(!s->deposit(0, err));
        CHECK(s->deposit(2550, err) && s->balance() == 52550);

        // Current account overdraft
        Account* c = bank.openAccount('c', "Ravi", "4321", 0, err);
        CHECK(c && c->number() == 1002);
        CHECK(c->withdraw(1000 * 100, err) && c->balance() == -100000);
        CHECK(!c->withdraw(10000 * 100, err));

        // Validation
        CHECK(bank.openAccount('s', "Bad|Name", "1234", 100000, err) == nullptr);
        CHECK(bank.openAccount('s', "X", "12", 100000, err) == nullptr);

        // PIN + lockout
        CHECK(s->verifyPin("1234"));
        CHECK(!s->verifyPin("0000"));
        CHECK(!s->verifyPin("0000"));
        CHECK(s->verifyPin("1234"));  // success resets counter
        CHECK(!s->verifyPin("1"));
        CHECK(!s->verifyPin("1"));
        CHECK(!s->verifyPin("1"));
        CHECK(s->locked());
        CHECK(!s->verifyPin("1234"));  // correct PIN rejected once locked

        CHECK(c->changePin("4321", "9999", err));
        CHECK(c->verifyPin("9999") && !c->verifyPin("4321"));
        CHECK(bank.save());
    }
    {
        // Persistence: reload from disk
        Bank bank(file);
        CHECK(bank.load() == 0);
        CHECK(bank.count() == 2);
        Account* s = bank.find(1001);
        Account* c = bank.find(1002);
        CHECK(s && s->holder() == "Asha" && s->balance() == 52550 && s->locked());
        CHECK(c && c->balance() == -100000 && c->type() == "Current");
        CHECK(c->verifyPin("9999") || c->locked());
        CHECK(bank.find(9999) == nullptr);
        // New account numbers continue after the highest existing one
        Account* n = bank.openAccount('c', "Neha", "1111", 0, err);
        CHECK(n && n->number() == 1003);
    }
    {
        // PIN must never be stored in plain text
        std::ifstream in(file);
        std::string all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        CHECK(all.find("|9999|") == std::string::npos);
        CHECK(all.find("|1234|") == std::string::npos);
    }
    {
        // Corrupt lines are skipped, not fatal
        std::ofstream out(file, std::ios::app);
        out << "garbage line\nSavings|abc|x|y|z|1|0\n";
    }
    {
        Bank bank(file);
        CHECK(bank.load() == 2);
        CHECK(bank.count() == 2);
    }

    std::filesystem::remove(file);
    if (failures == 0) std::cout << "All tests passed.\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
