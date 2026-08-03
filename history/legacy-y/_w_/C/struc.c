#include <stdio.h>

typedef struct A { int a, b, c, p, q, r; } A;
typedef struct B { int d, e, f, s, t, u; } B;

#define _B_ B b = {1, 2, 3, 4, 5, 6};
#define _A_ A a = {7, 8, 9, 10, 11, 12};
// B b = {1, 2, 3, 4, 5, 6};
// A a = {7, 8, 9, 10, 11, 12};
// #define _B_
// #define _A_

inline void f(B *b1, A *a2) {
  b1->d = a2->a;
  b1->e = a2->b;
  b1->f = a2->c;
  b1->s = a2->p;
  b1->t = a2->q;
  b1->u = a2->r;
}

// void f(B b1, A a2) {
//   b1.d = a2.a;
//   b1.e = a2.b;
//   b1.f = a2.c;
//   b1.s = a2.p;
//   b1.t = a2.q;
//   b1.u = a2.r;
// }

inline void g(B **b1, A **a2) {
  (*b1)->d = (*a2)->a;
  (*b1)->e = (*a2)->b;
  (*b1)->f = (*a2)->c;
  (*b1)->s = (*a2)->p;
  (*b1)->t = (*a2)->q;
  (*b1)->u = (*a2)->r;
}


void c(){
    _B_
    _A_
    f(&b, &a); //  f(&a, &b); ggc acusa erro
    printf("%d %d %d %d",a.a, b.d, a.r, b.u);
}

void d(){
    _B_
    _A_
    B *d = &b;
    A *e = &a;
    g(&d, &e);
    printf("%d %d %d %d",a.b, b.d, a.r, b.u);
}

// testar também com  -fwhole-program e -flto