# Crypto

## Requirements

- [conan](https://conan.io/) package manager

## Install

Run from within this folder:

```sh
conan create .
```

Which will create this package in the local conan cache.

## Usage

```toml
[requires]
dory-crypto/0.0.1

[options]
dory-crypto:log_level=<level>
```

refer to our [wiki](https://github.com/link_not_accessible_in_anon_submission/Logger) to
see the various log level options.

Use the lib in the source files as follows:

```cpp
#include <dory/crypto/sign.hpp>
```
