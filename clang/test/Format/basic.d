// RUN: grep -Ev "// *[A-Z-]+:" %s | clang-format -style=LLVM --assume-filename=%s | FileCheck -strict-whitespace %s

int main() {
    // CHECK: int x = cast(int*)&y;
    int x = cast(int*)    &y;
    // CHECK: int* z = a;
    int *z = a;
    // CHECK: int* z = a;
    int * z = a;
    // CHECK: int x = x * &a + *z;
    int x = x*&a+*z;

    // CHECK: if (x) {
    if(x){}
}

struct X {
    // CHECK: private void foo() {}
    private void foo() {}
    // CHECK: private void bar() {}
    private void bar() {}
}

// CHECK: void bar();
void bar();

void baz() {
    // CHECK: {{\ \ import foo.bar.baz;}}
import foo.bar.baz;
    int x;
}
