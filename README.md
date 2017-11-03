# ORM Lite

**ORM Lite** is a C++ [_**Object Relation Mapping** (ORM)_](https://en.wikipedia.org/wiki/Object-relational_mapping)
for **SQLite3** (currently 😂),
written in Modern C++ style.

[![Build status - MSVC](https://ci.appveyor.com/api/projects/status/github/BOT-Man-JL/ORM-Lite?svg=true&branch=master)](https://ci.appveyor.com/project/BOT-Man-JL/ORM-Lite)
[![Build status - gcc/clang](https://travis-ci.org/BOT-Man-JL/ORM-Lite.svg?branch=master)](https://travis-ci.org/BOT-Man-JL/ORM-Lite)

## Features

- **Easy** to Use
- **Header Only**
  ([src/ormlite.h](src/ormlite.h), [src/nullable.h](src/nullable.h))
- **Powerful** Compile-time **Type/DSL Deduction**

## Documentation

#### [Get Started Here](docs/get-started.md) 😉

#### [Full Document](docs/orm-lite.md) 😊

## Planned Features

- Support More Databases (Looking for a Better Driver recently...)
- Customized Primary Key (Hard to Design an Elegant Interface for it...)
- Blob (Hard to be Serialized to Script...)
- Date/Time Types (Weak Typed in SQL...)
- Subquery (Too Complicated... the Interface would be)

Feel free to [Issue](https://github.com/BOT-Man-JL/ORM-Lite/issues/new),
if you have any idea. 😎

## Implementation Details （实现细节）

Posts in **Chinese** only:

- [How to Design a Naive C++ ORM](https://BOT-Man-JL.github.io/articles/#2016/How-to-Design-a-Naive-Cpp-ORM)
- [How to Design a Better C++ ORM](https://BOT-Man-JL.github.io/articles/#2016/How-to-Design-a-Better-Cpp-ORM)
