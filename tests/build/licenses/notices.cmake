# The notices of one dependency, word for word.

include("${CMAKE_CURRENT_LIST_DIR}/fake_vcpkg.cmake")

fake_vcpkg_manifest(DEPENDENCIES alpha-lib)
fake_vcpkg_port(alpha-lib VERSION 1.2.0 LICENSE "MIT OR Apache-2.0" HOMEPAGE https://example.com/alpha)
fake_vcpkg_check()

file(READ "${FAKE_NOTICES}" notices)
set(expected [=[
# Third-party notices

aplomb is built on the libraries below, each under its own licence. A program that ships aplomb
carries the notices these licences ask for, which this file gathers.

This file is generated from vcpkg's metadata by the `third-party-notices` target. Don't edit it by
hand.

## alpha-lib 1.2.0

- Licence: MIT OR Apache-2.0
- Homepage: https://example.com/alpha

```text
Copyright of alpha-lib.
```
]=])
if(NOT notices STREQUAL expected)
    message(FATAL_ERROR "The notices should have been:\n${expected}\nbut were:\n${notices}")
endif()
