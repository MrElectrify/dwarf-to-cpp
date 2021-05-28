#include <DWARFToCPP/Parser.h>

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
		std::cout << "Usage: " << argv[0] << " <elf:path> <outFile:path>\n";
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
		if (const auto err = parser.Parse(d);
			err.has_value() == true)
		{
			std::cerr << "Failed to parse DWARF data: " << err.value() << '\n';
			return 1;
		}
		// open the output file
		std::ofstream outFile(argv[2]);
		if (outFile.good() == false)
		{
			std::cerr << "Failed to open output file " << argv[2] << '\n';
			return 1;
		}
		parser.Print(outFile);
	}
	catch (const std::exception& e)
	{
		std::cerr << "Failed to dump types: " << e.what() << '\n';
		return 1;
	}
	return 0;
}