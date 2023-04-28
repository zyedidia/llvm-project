// RUN: grep -Ev "// *[A-Z-]+:" %s | clang-format -style=LLVM --assume-filename=%s | FileCheck -strict-whitespace %s

// CHECK: void foo(const(char)* x);
void foo(const(char)* x);

// CHECK: void foo(int* x) {
void foo(int* x) {
    // CHECK: if (x * y) {
    if (x*y) {

    }

    // CHECK: void bar(int* x, type* y);
    void bar(int* x, type* y);
    // CHECK: type bar(int* z) {
    type bar(int* z) {
        int z;
        int z;
    }
}
