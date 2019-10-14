# Mockup

Mockup is a header-only mocking library for C++17.

**Mockup is in the very early stages of development, and may change substantially in the future. Use at your own risk.**

## Features

* All function calls are recorded and can be checked afterwards
* Relaxed by default (all calls are allowed; behaviours repeat)
* No macros (a single convenience macro may be provided to support C++11/14)
* No compiler-specific vtable magic
* And more TBD...

## Quick example

Given an interface:

```cpp
struct foo {
    virtual int bar(int x) = 0;
};
```

We can write a test class:

```cpp
#include <mockup/mockup.hpp>

using namespace mockup;

struct test_foo : foo {
    int bar(int x) override {
        return call<&foo::bar>(*this, x);
    }
};
```

And then write tests using mock objects:

```cpp
// Create our mock object.
mock<test_foo> mock_foo;

// Tell it to return 42 when passed 7.
mock_foo.when<&foo::bar>(7)(return_(42));

// The mock object will behave as instructed.
assert(mock_foo->bar(7) == 42);

// Check that the function was invoked as expected.
assert(mock_foo.invoked<&foo::bar>(7));

// We can also check using wildcards.
assert(mock_foo.invoked<&foo::bar>(_));
```
