#include <DWARFToCPP/Parser.h>

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
	// open the file
	int fd = open(argv[1], O_RDONLY);
	if (fd < 0)
	{
		std::cerr << "Failed to open file " << argv[1] << ": " << errno << '\n';
		return 1;
	}
	try
	{
		elf::elf e(elf::create_mmap_loader(fd));
		dwarf::dwarf d(dwarf::elf::create_loader(e));
		// create a parser
		DWARFToCPP::Parser parser;
		if (const auto err = parser.ParseDWARF(d);
			err.has_value() == true)
		{
			std::cerr << "Failed to parse DWARF data: " << err.value() << '\n';
			return 1;
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << "Failed to dump types: " << e.what() << '\n';
		return 1;
	}
	return 0;
}