#include <stdio.h>

int *f(int *a) {
    return a;
}

int main() {
    int a;
    int *b = &a;
    int *c = f(b);
    return 0;
}
