#include <stdio.h>

struct file1 {
    int a;
    int b;
    const char * hello;
} self = {
    .a = 0,
    .b = 1,
    .hello = u8"Hello Module 😎"
};

typedef struct file1 file1;

const static inline file1* file1_replace(file1 *self, int a, int b) { 
    self->a = a;
    self->b = b;
    return self;
}

__attribute__((pure)) const static inline int file1_getA(file1 *self) { 
    return self->a;
}

__attribute__((pure)) const static inline int file1_getB(file1 *self) { 
    return self->b;
}

const struct file1_exports {
    const file1 *self;
    const file1* (*replace)(file1 *self, int a, int b); 
    const int (*getA)(file1 *self); 
    const int (*getB)(file1 *self); 

} export = {
        .self = &self,
        .replace = file1_replace,
        .getA = file1_getA,
        .getB = file1_getB
        };

typedef const struct file1_exports file1_exports;

__attribute__((const)) extern const file1_exports exports(){
    // contructor??
    return export;
}

/* int main(){
    const file1_exports module = exports();
    printf("%s\n", module.self->hello);
    printf("%d %d\n", module.getA(&self), module.getB(&self));
    module.replace(&self, 2, 3);
    printf("%d %d\n", module.getA(&self), module.getB(&self));
    return 0;
}
*/