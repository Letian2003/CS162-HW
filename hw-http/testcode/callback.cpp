#include <stdio.h>
#include <functional>
#include <iostream>

using namespace std;

typedef int(*FP)(int,int);

typedef std::function<int(int,int)> fun;

int add(int a,int b){
    return a+b;
}

auto mul = [](int a,int b){
    return a*b;
};

int main(){
    fun f1 = add;
    fun f2 = mul;

    int a = add(10,4);
    int b= mul(6,1234);

    cout<<a<<" "<<b<<endl;

    return 0;
}