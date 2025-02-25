#include <stdio.h>

struct file1 {
    int a;
    int b;
    const char * hello;
};

struct file1_exports {
    struct file1 *self;
    struct file1* (*replace)(struct file1 *self, int a, int b); 
    const int (*getA)(struct file1 *self); 
    const int (*getB)(struct file1 *self); 

};

extern const struct file1_exports exports();

int main(int argc, char** argv){ // int wmain(int argc, wchar_t** argv)
    const struct file1_exports module = exports();
    printf("%s\n", module.self->hello);
    printf("%d %d\n", module.getA(module.self), module.getB(module.self));
    module.replace(module.self, 2, 3);
    printf("%d %d\n", module.getA(module.self), module.getB(module.self));
    return 0;
}