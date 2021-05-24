#include <elf++.hh>
#include <dwarf++.hh>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>

#ifdef _WIN32
#include <io.h>
#endif

#include <iostream>

int main(int argc, char* argv[])
{
	if (argc != 2)
	{
		std::cout << "Usage: " << argv[0] << " <elf:path>\n";
		return 1;
	}
	return 0;
}