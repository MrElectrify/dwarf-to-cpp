add_executable(dwarf-to-cpp dwarf-to-cpp.cpp)

target_link_libraries(dwarf-to-cpp
	PUBLIC libelfin::libdwarf)