# Bank Management System (C++)

A console-based banking application written in modern C++17. It simulates core banking operations — **deposit, withdrawal and balance inquiry** — using object-oriented design, and keeps customer records on disk so they survive between runs.

## Features

- Open **Savings** or **Current** accounts (auto-assigned account numbers)
- **Deposit**, **withdraw**, **check balance**, **change PIN**
- **Persistent storage** in a plain-text data file (`data/bank.dat`), saved after every change
- **PIN security**
  - PINs are never stored: each is hashed with **SHA-256 + a per-account random salt**
  - PIN entry is hidden in the terminal
  - Account **locks after 3 wrong PINs** (the counter is persisted, so restarting the program doesn't reset it)
  - Login failures use one generic message so account numbers can't be probed
- Money is stored as integer **paise**, so there are no floating-point rounding errors
- Crash-safe saves (write to a temp file, then atomically rename); data file is owner-only where the OS supports it
- Corrupt lines in the data file are skipped instead of crashing the program

## OOP concepts used

| Concept | Where |
|---|---|
| Abstraction | `Account` is an abstract base class with pure virtual `type()` and `withdrawLimit()` |
| Inheritance | `SavingsAccount` and `CurrentAccount` derive from `Account` |
| Polymorphism | `withdraw()` works through `withdrawLimit()`: Savings must keep a **Rs. 500** minimum balance; Current allows an **Rs. 10,000** overdraft |
| Encapsulation | Balance, salt and PIN hash are `protected`; only validated methods change them |
| Factory | `Account::create()` builds the right subclass |
| Composition / ownership | `Bank` owns accounts via `std::unique_ptr` in a `std::map` |
| File handling | `Bank::load()` / `Bank::save()` using `std::ifstream` / `std::ofstream` |

## Project structure

```
bank-management-system/
├── include/
│   ├── Account.h      # Account, SavingsAccount, CurrentAccount
│   ├── Bank.h         # account registry + file persistence
│   └── sha256.h       # header-only SHA-256
├── src/
│   ├── Account.cpp
│   ├── Bank.cpp
│   └── main.cpp       # console menu
├── tests/
│   └── test_bank.cpp  # dependency-free test suite
├── Makefile
└── README.md
```

## Build and run

Requires a C++17 compiler (g++ or clang++).

```bash
make          # build -> build/bank
make run      # build and start the app
make test     # run the test suite
```

Without `make`:

```bash
g++ -std=c++17 -Iinclude src/*.cpp -o bank
./bank                 # uses data/bank.dat
./bank mydata.dat      # or choose another data file
```

## Data file format

One account per line, pipe-separated:

```
type|accountNumber|holderName|salt|pinHash|balanceInPaise|failedAttempts
Savings|1001|Asha|3fa9...|9c1d...|52550|0
```

## Security notes

This is a learning project. It protects PINs properly (salted hashes, lockout), but the data file itself is **not encrypted** — balances and names are readable by anyone with access to the file, and the app has no admin role to unlock accounts. For a real system you would use a database, encryption at rest, a slow password hash (bcrypt/argon2), and audit logs. `data/` is in `.gitignore` so customer records are never pushed to GitHub.

## Possible extensions

Transaction history / mini-statement, fund transfer between accounts, interest calculation, admin login to unlock accounts, account closure.

## License

MIT (add a `LICENSE` file to your repo).
