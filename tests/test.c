#include <stdio.h>
#include <stdlib.h>

int *f(int *a) {
    return a;
}

int main() {
    int a;
    int *b = &a;
    int *c = f(b);
    int *d = malloc(10);

    return 0;
}
