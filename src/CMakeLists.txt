add_library(Parser "Parser.cpp")

target_link_libraries(Parser
	PUBLIC tl::expected
	PUBLIC libelfin::libdwarf)

SET_PROJECT_WARNINGS(Parser)

target_include_directories(Parser
	PUBLIC ../include)

add_library(DWARFToCPP::Parser ALIAS Parser)