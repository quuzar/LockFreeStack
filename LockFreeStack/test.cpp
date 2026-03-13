#include "allocator_test.h"
#include "lockfreestack_test.h"

#include <iostream>
int main() {
	std::cout << "START TEST" << std::endl;
	allocator_test();
	stack_test();
	std::cout << "END TEST\n" << std::endl;
}