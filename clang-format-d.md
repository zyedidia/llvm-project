# Notes for clang-format-d

## Building

First configure the project. You will need `clang` and `lld` (or you can build
with GNU tools, just don't provide the additional flags for changing the
compiler/linker).

```
mkdir build
cd build
cmake -G Ninja ../llvm \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DLLVM_ENABLE_PROJECTS=clang \
    -DLLVM_ENABLE_ASSERTIONS=ON \
    -DLLVM_ENABLE_TERMINFO=OFF \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DLLVM_USE_LINKER=lld
```

Now build (with `ninja`):

```
ninja bin/clang-format
```

## Testing

To run the tests:

```
ninja check-clang-format
```

The tests are located in `clang/test/Format`:

* `basic.d`
* `extern.d`
* `functions.d`
* `template.d`

Currently a number of tests are failing because clang-format-d is not complete.
If you find additional cases of clang-format acting strangely, please create a
new test (or add to an existing one), and open a PR. If you write a fix, please
open a PR as well!

You can also manually run `bin/clang-format` (after building) on one of the
test files, or any D file of your own.

## Developing

The source code is in `clang/lib/Format` and there is a header in
`clang/include/clang/Format`. The code consists of an ad-hoc parser and a lot
of case-specific checking and formatting, with conditionals for
language-specific formatting (not the cleanest organization).
