#include "aare.hpp"
#include <iostream>
#include <functional>
#include <vector>


void function_factory(int i){
    std::cout << "Function " << i << " is running" << std::endl;
    int64_t count  = 0;
    for (int j = 0; j < 3e9; j++){
        count += j;

    }
    std::cout << "Function " << i << " is done" << std::endl;
}

int main(){

    std::vector<std::function<void()>> functions;
    for (int i = 0; i < 10; i++){
        functions.push_back(std::bind(function_factory, i));
    }
    

    aare::MultiThread mt(functions);
    mt.run();



    return 0;
}
