#include <bits/stdc++.h>

class A {
public:
    void fn() { printf("fn in A\n"); }
    virtual void v_fn() { printf("virtual fn in A\n"); }
};

class B : public A {
public:
    void fn() { printf("fn in B\n"); }
    virtual void v_fn() { printf("virtual fn in B\n"); }
};

int main() {
    A *a = new B();
    a->fn();
    a->v_fn();
	std::make_pair(a,a);
    return 0;
}
