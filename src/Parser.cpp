#include <DWARFToCPP/Parser.h>

#include <fmt/format.h>

#include <ranges>
#include <stack>
#include <unordered_set>

using namespace DWARFToCPP;

// types

void BaseType::Print(std::ostream& out, size_t indentLevel) const noexcept
{

}

std::optional<std::string> Class::Parse(Parser& parser, const dwarf::die& entry) noexcept
{
	if (const auto nameRes = NamedConcept::Parse(parser, entry);
		nameRes.has_value() == true)
		return nameRes;
	// parse each child in the class
	for (const auto child : entry)
	{
		if (child.tag != dwarf::DW_TAG::formal_parameter &&
			child.tag != dwarf::DW_TAG::template_type_parameter &&
			child.tag != dwarf::DW_TAG::template_value_parameter)
			continue;
		auto parsedChild = parser.Parse(child);
		if (parsedChild.has_value() == false)
			return std::move(parsedChild.error());
		if (child.tag == dwarf::DW_TAG::formal_parameter)
			AddConcept(parsedChild.value());
		else
		{
			if (child.tag == dwarf::DW_TAG::template_type_parameter)
				AddTemplateParameter(std::dynamic_pointer_cast<TemplateType>(
					std::move(parsedChild.value())));
		}
	}
	return std::nullopt;
}

void Class::Print(std::ostream& out, size_t indentLevel) const noexcept
{

}

void Const::Print(std::ostream& out, size_t indentLevel) const noexcept
{

}

std::optional<std::string> Instance::Parse(Parser& parser, const dwarf::die& entry) noexcept
{
	if (const auto nameRes = NamedConcept::Parse(parser, entry);
		nameRes.has_value() == true)
		return nameRes;
	auto type = entry.resolve(dwarf::DW_AT::type);
	if (type.valid() == false)
		return "An instance did not have a type";
	auto parsedType = parser.Parse(type.as_reference());
	if (parsedType.has_value() == false)
		return std::move(parsedType.error());
	if (parsedType.value()->GetConceptType() != ConceptType::Type)
		return "An instance's type was not a type";
	m_instanceType = std::dynamic_pointer_cast<Type>(parsedType.value());
	return std::nullopt;
}

void Instance::Print(std::ostream& out, size_t indentLevel) const noexcept
{

}

std::optional<std::string> Modifier::Parse(Parser& parser, const dwarf::die& entry) noexcept
{
	auto referencedType = entry.resolve(dwarf::DW_AT::type);
	if (referencedType.valid() == false)
	{
		if (m_allowVoid == true)
			return std::nullopt;
		return "A modifier had a void type, and was not allowed";
	}
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
		m_name = fmt::format_int(std::hash<void*>()(this)).str();
	else
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

std::optional<std::string> SubProgram::Parse(Parser& parser, const dwarf::die& entry) noexcept
{
	if (const auto error = SubRoutine::Parse(parser, entry);
		error.has_value() == true)
		return error;
	// look for template types
	for (const auto child : entry)
	{
		if (child.tag != dwarf::DW_TAG::template_type_parameter &&
			child.tag != dwarf::DW_TAG::template_value_parameter)
			continue;
		auto parsedTemplateParam = parser.Parse(child);
		if (parsedTemplateParam.has_value() == false)
			return std::move(parsedTemplateParam.error());
		if (child.tag == dwarf::DW_TAG::template_type_parameter)
			AddTemplateParameter(std::dynamic_pointer_cast<
				TemplateType>(std::move(parsedTemplateParam.value())));
	}
	return std::nullopt;
}

void SubProgram::Print(std::ostream& out, size_t indentLevel) const noexcept
{

}

std::optional<std::string> SubRoutine::Parse(Parser& parser, const dwarf::die& entry) noexcept
{
	auto returnType = entry.resolve(dwarf::DW_AT::type);
	if (returnType.valid() == true)
	{
		auto parsedReturnType = parser.Parse(returnType.as_reference());
		if (parsedReturnType.has_value() == false)
			return std::move(parsedReturnType.error());
		if (parsedReturnType.value()->GetConceptType() != ConceptType::Type)
			return "A subroutine return type was not a type";
		m_returnType = std::dynamic_pointer_cast<Type>(parsedReturnType.value());
	}
	for (const auto child : entry)
	{
		if (child.tag != dwarf::DW_TAG::formal_parameter)
			continue;
		auto parsedChild = parser.Parse(child);
		if (parsedChild.has_value() == false)
			return std::move(parsedChild.error());
		if (parsedChild.value()->GetConceptType() != ConceptType::Instance)
			return "A subroutine parameter was not an instance";
		m_parameterTypes.emplace_back(std::dynamic_pointer_cast<Instance>(
			std::move(parsedChild.value())));
	}
	return std::nullopt;
}

void SubRoutine::Print(std::ostream& out, size_t indentLevel) const noexcept
{

}

void Templated::AddTemplateParameter(std::variant<std::weak_ptr<TemplateType>> templateParameter) noexcept
{
	m_templateParameters.push_back(std::move(templateParameter));
}

std::optional<std::string> TemplateValue::Parse(Parser& parser, const dwarf::die& entry) noexcept
{
	if (const auto error = Instance::Parse(parser, entry);
		error.has_value() == true)
		return error;
	const auto value = entry.resolve(dwarf::DW_AT::const_value);
	if (value.valid() == false)
		return "A template type was missing a value";
	switch (value.get_type())
	{
	case dwarf::value::type::constant:
	case dwarf::value::type::uconstant:
		m_value = fmt::format_int(value.as_uconstant()).str();
		break;
	case dwarf::value::type::sconstant:
		m_value = fmt::format_int(value.as_sconstant()).str();
		break;
	case dwarf::value::type::string:
		m_value = value.as_string();
		break;
	default:
		return "Unhandled type for template value: " + to_string(value.get_type());
	}
	return std::nullopt;
}

void TemplateValue::Print(std::ostream& out, size_t indentLevel) const noexcept
{

}

std::optional<std::string> TypeDef::Parse(Parser& parser, const dwarf::die& entry) noexcept
{
	if (const auto error = NamedConcept::Parse(parser, entry);
		error.has_value() == true)
		return error;
	const auto aliasType = entry.resolve(dwarf::DW_AT::type);
	if (aliasType.valid() == false)
		return "A typedef did not have an associated type";
	auto parsedAliasType = parser.Parse(aliasType.as_reference());
	if (parsedAliasType.has_value() == false)
		return std::move(parsedAliasType.error());
	if (parsedAliasType.value()->GetConceptType() != ConceptType::Type)
		return "A typedef's type was not a type";
	m_aliasType = std::dynamic_pointer_cast<Type>(std::move(parsedAliasType.value()));
	return std::nullopt;
}

void TypeDef::Print(std::ostream& out, size_t indentLevel) const noexcept
{

}

void Reference::Print(std::ostream& out, size_t indentLevel) const noexcept
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
	case dwarf::DW_TAG::base_type:
		result = std::make_shared<BaseType>();
		break;
	case dwarf::DW_TAG::class_type:
	case dwarf::DW_TAG::structure_type:
	case dwarf::DW_TAG::union_type:
		result = std::make_shared<Class>();
		break;
	case dwarf::DW_TAG::const_type:
		result = std::make_shared<Const>();
		break;
	case dwarf::DW_TAG::formal_parameter:
	case dwarf::DW_TAG::member:
		result = std::make_shared<Instance>();
		break;
	case dwarf::DW_TAG::namespace_:
		result = std::make_shared<Namespace>();
		break;
	case dwarf::DW_TAG::pointer_type:
		result = std::make_shared<Pointer>();
		break;
	case dwarf::DW_TAG::reference_type:
		result = std::make_shared<Reference>();
		break;
	case dwarf::DW_TAG::subprogram:
		result = std::make_shared<SubProgram>();
		break;
	case dwarf::DW_TAG::subroutine_type:
		result = std::make_shared<SubRoutine>();
		break;
	case dwarf::DW_TAG::template_type_parameter:
		result = std::make_shared<TemplateType>();
		break;
	case dwarf::DW_TAG::template_value_parameter:
		result = std::make_shared<TemplateValue>();
		break;
	case dwarf::DW_TAG::typedef_:
		result = std::make_shared<TypeDef>();
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