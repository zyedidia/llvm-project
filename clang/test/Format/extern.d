// RUN: grep -Ev "// *[A-Z-]+:" %s | clang-format -style=LLVM --assume-filename=%s | FileCheck -strict-whitespace %s

// CHECK: extern (C) {
extern (C) {
    // CHECK: {{\ \ int foo;}}
    int foo;
    int bar;
}

// CHECK: version (GNU) {
version (GNU) {
    // CHECK: {{^\ \ int foo;}}
    int foo;
    int bar;
}

// CHECK: extern (C):
extern (C):
// CHECK: {{^int foo;}}
int foo;
// CHECK: {{^int bar;}}
int bar;

// CHECK: static if (x * y) {
static if(x*y) {
    int x;
// CHECK: } static else if (b) {
}static else if (b) {
    int x;
// CHECK: } else {
}else{
    int x;
// CHECK: }
}
