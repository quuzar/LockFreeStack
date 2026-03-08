#include "Stack.h"
#include <iostream>

int main() {
	LockStack<int> stack{};
	stack.push(10);
	stack.push(20);
	stack.push(30);
	stack.push(40);
	stack.push(50);

	std::cout << stack.pop() << std::endl;
	std::cout << stack.pop() << std::endl;
	std::cout << stack.pop() << std::endl;
	std::cout << stack.pop() << std::endl;
	std::cout << stack.pop() << std::endl;

	return 0;
}