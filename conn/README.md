# Connection

## Requirements

- [conan](https://conan.io/) package manager

## Install

Run from within this folder:

```sh
conan create .
```

Which will create this package in the local conan cache.

## Usage

Inside a `conanfile.txt` specify:

```toml
[requires]
dory-memstore/0.0.1

[options]
dory-connection:log_level=<level>
```

refer to our [wiki](https://github.com/link_not_accessible_in_anon_submission/Logger) to
see the various log level options.

Use the lib in the source files as follows:

```cpp
#include <dory/conn/rc.hpp>
#include <dory/conn/exchanger.hpp>
```
