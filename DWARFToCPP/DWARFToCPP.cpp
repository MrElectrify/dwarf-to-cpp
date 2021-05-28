#include <DWARFToCPP/Parser.h>

#include <fmt/format.h>

#include <elf++.hh>
#include <dwarf++.hh>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>

#ifdef _WIN32
#include <io.h>
#endif

// this warning is unavoidable. I didn't want to change the
// api of libelfin, and it was a POSIX library
#ifdef _WIN32
#pragma warning(disable : 4996)
#endif

#include <fstream>
#include <iostream>

int main(int argc, char* argv[])
{
	if (argc != 3)
	{
		fmt::print(stderr, "Usage: {} <elf:path> <outFile:path>\n", argv[0]);
		return 1;
	}
	// open the file
	int fd = open(argv[1], O_RDONLY);
	if (fd < 0)
	{
		fmt::print(stderr, "Failed to open input ELF file {}: {} ({})", argv[1], 
			std::strerror(errno), errno);
		return 1;
	}
	try
	{
		elf::elf e(elf::create_mmap_loader(fd));
		dwarf::dwarf d(dwarf::elf::create_loader(e));
		// create a parser
		DWARFToCPP::Parser parser;
		if (const auto err = parser.Parse(d);
			err.has_value() == true)
		{
			fmt::print(stderr, "Failed to parse DWARF structures: {}", err.value());
			return 1;
		}
		// open the output file
		/*std::ofstream outFile(argv[2]);
		if (outFile.good() == false)
		{
			fmt::print(stderr, "Failed to open output file {}", argv[2]);
			return 1;
		}*/
		parser.Print(std::cout);
	}
	catch (const std::exception& e)
	{
		fmt::print(stderr, "Failed to parse ELF/DWARF: {}", e.what());
		return 1;
	}
	return 0;
}