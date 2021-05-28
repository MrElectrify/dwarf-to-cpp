#include <DWARFToCPP/Parser.h>

#include <fmt/format.h>

#include <ranges>
#include <stack>
#include <unordered_set>

using namespace DWARFToCPP;

// types

std::optional<std::string> Class::Parse(Parser& parser, const dwarf::die& entry) noexcept
{
	if (const auto nameRes = NamedConcept::Parse(parser, entry);
		nameRes.has_value() == true)
		return nameRes;
	// parse each child in the class
	for (const auto child : entry)
	{
		auto parsedChild = parser.Parse(child);
		if (parsedChild.has_value() == false)
			return std::move(parsedChild.error());
		AddConcept(parsedChild.value());
	}
	return std::nullopt;
}

void Class::Print(std::ostream& out, size_t indentLevel) const noexcept
{

}

void Const::Print(std::ostream& out, size_t indentLevel) const noexcept
{

}

std::optional<std::string> Modifier::Parse(Parser& parser, const dwarf::die& entry) noexcept
{
	auto referencedType = entry.resolve(dwarf::DW_AT::type);
	if (referencedType.valid() == false)
		return "A modifier was missing a type";
	// parse the type
	auto parsedReferencedType = parser.Parse(referencedType.as_reference());
	if (parsedReferencedType.has_value() == false)
		return std::move(parsedReferencedType.error());
	if (parsedReferencedType.value()->GetConceptType() != ConceptType::Type)
		return "A modifier's type was not a type";
	m_referencedType = std::dynamic_pointer_cast<Type>(parsedReferencedType.value());
	return std::nullopt;
}

std::optional<std::string> NamedConcept::Parse(Parser& parser, const dwarf::die& entry) noexcept
{
	const auto name = entry.resolve(dwarf::DW_AT::name);
	if (name.valid() == false)
		return "The named concept did not have a name";
	m_name = name.as_string();
	return std::nullopt;
}

bool NamedConceptMap::AddConcept(std::shared_ptr<LanguageConcept> languageConcept) noexcept
{
	if (languageConcept->IsNamed() == false)
		return false;
	AddConcept(std::dynamic_pointer_cast<NamedConcept>(std::move(languageConcept)));
	return true;
}

void NamedConceptMap::AddConcept(const std::shared_ptr<NamedConcept>& namedConcept) noexcept
{
	m_namedConcepts.emplace(namedConcept->GetName(), namedConcept);
}

std::optional<std::string> Namespace::Parse(Parser& parser, const dwarf::die& entry) noexcept
{
	if (const auto nameRes = NamedConcept::Parse(parser, entry);
		nameRes.has_value() == true)
		return nameRes;
	// parse each child of the namespace. those are the members
	for (const auto child : entry)
	{
		auto parsedChild = parser.Parse(child);
		if (parsedChild.has_value() == false)
			return std::move(parsedChild.error());
		AddConcept(parsedChild.value());
	}
	return std::nullopt;
}

void Namespace::Print(std::ostream& out, size_t indentLevel) const noexcept
{
	
}

void Pointer::Print(std::ostream& out, size_t indentLevel) const noexcept
{

}

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
		else
			AddConcept(res.value());
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
	case dwarf::DW_TAG::class_type:
	case dwarf::DW_TAG::structure_type:
	case dwarf::DW_TAG::union_type:
		result = std::make_shared<Class>();
		break;
	case dwarf::DW_TAG::const_type:
		result = std::make_shared<Const>();
		break;
	case dwarf::DW_TAG::namespace_:
		result = std::make_shared<Namespace>();
		break;
	case dwarf::DW_TAG::pointer_type:
		result = std::make_shared<Pointer>();
		break;
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