// RUN: grep -Ev "// *[A-Z-]+:" %s | clang-format -style=LLVM --assume-filename=%s | FileCheck -strict-whitespace %s

// CHECK: void foo(T)(ulong a, ulong a, {{.*}},
// CHECK-NEXT: {{.*}}, ulong a, ulong a);
void foo(T)(ulong a,ulong a,ulong a,ulong a,ulong a,ulong a,ulong a,ulong a,ulong a,ulong a,ulong a,ulong a,ulong a);

// CHECK: struct FooBar(T) {
struct FooBar(T) {

}

struct X {
    // CHECK: enum foo = 12;
    enum foo = 12;
}
