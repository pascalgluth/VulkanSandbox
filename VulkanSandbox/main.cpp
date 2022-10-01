#include <iostream>

#include "Engine.h"

int main(int argv, char** arg)
{
	std::cout << "Starting..." << std::endl;
	
	Engine::Init();

	std::cin.get();

	return 0;
}