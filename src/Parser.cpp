#include <DWARFToCPP/Parser.h>

#include <fmt/format.h>

#include <ranges>
#include <stack>
#include <unordered_set>

using namespace DWARFToCPP;

// types



// parser

std::optional<std::string> Parser::Parse(const dwarf::dwarf& data) noexcept
{
	size_t unitNo = 1;
	for (const auto& compilationUnit : data.compilation_units())
	{
		size_t startingTypes = m_parsedConcepts.size();
		if (auto res = Parse(compilationUnit); 
			res.has_value() == true)
			return std::move(res.value());
		size_t currentTypes = m_parsedConcepts.size();
		size_t deltaTypes = currentTypes - startingTypes;
		printf("Parsed unit %zd/%zd with %zd new types and %zd total\n",
			unitNo++, data.compilation_units().size(), deltaTypes, m_parsedConcepts.size());
	}
	return std::nullopt;
}

std::optional<std::string> Parser::Parse(const dwarf::compilation_unit& unit) noexcept
{
	for (const auto& die : unit.root())
	{
		if (auto res = Parse(die); res.has_value() == false)
			return std::move(res.error());
	}
	return std::nullopt;
}

tl::expected<std::shared_ptr<LanguageConcept>, std::string> Parser::Parse(const dwarf::die& die) noexcept
{
	// if we already parsed it, return the entry. use unit and offset to save space
	const auto parsedIt = m_parsedConcepts.find(
		reinterpret_cast<const char*>(
		&die.get_unit()) + die.get_section_offset());
	if (parsedIt != m_parsedConcepts.end())
		return parsedIt->second;
	std::shared_ptr<LanguageConcept> result;
	// todo: make a self-registering factory for this
	switch (die.tag)
	{
	default:
		return tl::make_unexpected("Unimplemented DIE type " + to_string(die.tag));
	}
	m_parsedConcepts.emplace(reinterpret_cast<const char*>(
			&die.get_unit()) + die.get_section_offset(), result);
	if (auto parseRes = result->Parse(*this, die);
		parseRes.has_value() == true)
		return tl::make_unexpected(std::move(parseRes.value()));
	return std::move(result);
}

void Parser::Print(std::ostream& out) noexcept
{
	// print the global namespace
}