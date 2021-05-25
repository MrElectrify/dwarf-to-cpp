add_executable(DWARFToCPP DWARFToCPP.cpp)

target_link_libraries(DWARFToCPP
	PUBLIC DWARFToCPP::Parser)