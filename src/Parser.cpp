#include <DWARFToCPP/Parser.h>

#include <ranges>
#include <stack>
#include <unordered_set>

#include <iostream>

using namespace DWARFToCPP;

// types

std::optional<std::string> Array::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	// find the type
	auto type = die.resolve(dwarf::DW_AT::type);
	if (type.valid() == false)
		return "An array was missing a type!";
	// parse the type
	auto parsedType = parser.ParseDIE(type.as_reference());
	if (parsedType.has_value() == false)
		return std::move(parsedType.error());
	if (parsedType->get()->GetType() != Type::Typed)
		return "An array's type was not a type!";
	m_type = std::static_pointer_cast<Typed>(std::move(parsedType.value()));
	// get the child, which contains size
	auto child = *die.begin();
	if (child.tag != dwarf::DW_TAG::subrange_type)
		return "An array was missing its subrange info!";
	// parse the size
	auto size = child.resolve(dwarf::DW_AT::upper_bound);
	if (size.valid() == false)
		return "An array's subrange info was missing the size!";
	// the subrange size + 1 is the array's size
	m_size = size.as_uconstant();
	return std::nullopt;
}

std::optional<std::string> BasicType::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == false)
		return "A basic type was missing a name!";
	Name(name.as_string());
	return std::nullopt;
}

std::optional<std::string> Class::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	m_classType = die.tag;
	auto name = die.resolve(dwarf::DW_AT::name);
	std::string className;
	if (name.valid() == true)
		Name(name.as_string());
	bool publicDefault = (die.tag != dwarf::DW_TAG::class_type);
	// a namespace contains many children. parse each one
	for (auto child : die)
	{
		// if accessibility is unstated, it uses the defaults
		Accessibility accessibility = (publicDefault == true) ? Accessibility::Public : Accessibility::Private;
		auto accessibilityAttr = child.resolve(dwarf::DW_AT::accessibility);
		if (accessibilityAttr.valid() == true)
			accessibility = static_cast<Accessibility>(accessibilityAttr.as_uconstant());
		if (child.tag == dwarf::DW_TAG::inheritance)
		{
			auto inheritanceType = child.resolve(dwarf::DW_AT::type);
			if (inheritanceType.valid() == false)
				return "An class inheritance did not have a type!";
			auto parsedInheritanceType = parser.ParseDIE(inheritanceType.as_reference());
			if (parsedInheritanceType.has_value() == false)
				return std::move(parsedInheritanceType.error());
			if (parsedInheritanceType.value()->GetType() != Type::Typed)
				return "A class inheritance was not a type!";
			const auto parentClass = std::static_pointer_cast<Typed>(
				std::move(parsedInheritanceType.value()));
			// ensure it is also a class
			if (parentClass->GetTypeCode() != TypeCode::Class)
				return "A class inheritance was not a class!";
			m_parentClasses.emplace_back(std::static_pointer_cast<Class>(parentClass), accessibility);
			continue;
		}
		// the child is a type. parse it
		auto parsedChild = parser.ParseDIE(child);
		if (parsedChild.has_value() == false)
			return std::move(parsedChild.error());
		// make sure the type is not a namespace
		if (parsedChild.value()->GetType() == Type::Namespace)
			return "A class had a nested namespace!";
		if (child.tag == dwarf::DW_TAG::template_type_parameter ||
			child.tag == dwarf::DW_TAG::template_value_parameter)
		{
			// make sure it's a value type
			if (parsedChild.value()->GetType() != Type::Value)
				return "A class had an invalid template type!";
			m_templateParameters.push_back(std::static_pointer_cast<Value>(
				std::move(parsedChild.value())));
			continue;
		}
		// it's a normal member
		m_members.emplace_back(parsedChild.value(), accessibility);
	}
	return std::nullopt;
}

std::optional<std::string> ConstType::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	// parse the embedded type
	auto type = die.resolve(dwarf::DW_AT::type);
	if (type.valid() == false)
		return "A const type did not have a type!";
	auto parsedType = parser.ParseDIE(type.as_reference());
	if (parsedType.has_value() == false)
		return std::move(parsedType.error());
	if (parsedType.value()->GetType() != Type::Typed)
		return "A const type was not a type!";
	m_type = std::static_pointer_cast<Typed>(std::move(parsedType.value()));
	return std::nullopt;
}

std::optional<std::string> Enum::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == false)
		return "An enum was missing a name!";
	Name(name.as_string());
	// parse the enumerators
	for (auto child : die)
	{
		auto enumerator = parser.ParseDIE(child);
		if (enumerator.has_value() == false)
			return std::move(enumerator.error());
		if (enumerator.value()->GetType() != Type::Enumerator)
			return "An enum had a non-enumerator child!";
		m_enumerators.push_back(std::static_pointer_cast<Enumerator>(
			std::move(enumerator.value())));
	}
	return std::nullopt;
}

std::optional<std::string> Enumerator::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == false)
		return "An enumerator was missing a name!";
	Name(name.as_string());
	auto value = die.resolve(dwarf::DW_AT::const_value);
	if (value.valid() == false)
		return "An enumerator was missing a value!";
	m_value = value.as_uconstant();
	return std::nullopt;
}

void Namespace::AddNamed(const std::shared_ptr<Named>& named) noexcept
{
	if (named == nullptr)
		return;
	auto name = named->Name();
	m_namedConcepts.emplace(std::move(name), named);
}

std::optional<std::string> Namespace::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == false)
		return "A namespace was missing a name!";
	Name(name.as_string());
	// a namespace contains many children. parse each one
	for (const auto& child : die)
	{
		// parse the child
		auto parsedChild = parser.ParseDIE(child);
		if (parsedChild.has_value() == false)
			return std::move(parsedChild.error());
		auto childName = parsedChild.value()->Name();
		m_namedConcepts.emplace(std::move(childName), parsedChild.value());
	}
	return std::nullopt;
}

std::optional<std::string> Pointer::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	auto type = die.resolve(dwarf::DW_AT::type);
	if (type.valid() == false)
		return "A pointer was missing a type!";
	auto parsedType = parser.ParseDIE(type.as_reference());
	if (parsedType.has_value() == false)
		return std::move(parsedType.error());
	if (parsedType.value()->GetType() != Type::Typed)
		return "A pointer was not in reference to a type!";
	m_type = std::static_pointer_cast<Typed>(std::move(parsedType.value()));
	return std::nullopt;
}

std::optional<std::string> PointerToMember::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	auto containingType = die.resolve(dwarf::DW_AT::containing_type);
	if (containingType.valid() == false)
		return "A pointer-to-member was missing a containing type!";
	auto parsedContainingNamed = parser.ParseDIE(containingType.as_reference());
	if (parsedContainingNamed.has_value() == false)
		return std::move(parsedContainingNamed.error());
	if (parsedContainingNamed.value()->GetType() != Type::Typed)
		return "A pointer-to-member had a non-typed containing type!";
	auto parsedContainingType = std::static_pointer_cast<Typed>(std::move(parsedContainingNamed.value()));
	if (parsedContainingType->GetTypeCode() != TypeCode::Class)
		return "A pointer-to-member's containing type was not class-based!";
	m_containingType = std::static_pointer_cast<Class>(std::move(parsedContainingType));
	auto functionType = die.resolve(dwarf::DW_AT::type);
	if (functionType.valid() == false)
		return "A pointer-to-member was missing a function type!";
	auto parsedFunctionType = parser.ParseDIE(functionType.as_reference());
	if (parsedFunctionType.has_value() == false)
		return std::move(parsedFunctionType.error());
	if (parsedFunctionType.value()->GetType() != Type::SubProgram)
		return "A pointer-to-member had a non-type function!";
	m_functionType = std::static_pointer_cast<SubProgram>(std::move(parsedFunctionType.value()));
	return std::nullopt;
}

std::optional<std::string> RefType::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	// parse the embedded type
	auto type = die.resolve(dwarf::DW_AT::type);
	if (type.valid() == false)
		return "A const type did not have a type!";
	auto parsedType = parser.ParseDIE(type.as_reference());
	if (parsedType.has_value() == false)
		return std::move(parsedType.error());
	if (parsedType.value()->GetType() != Type::Typed)
		return "A const type was not a type!";
	m_type = std::static_pointer_cast<Typed>(std::move(parsedType.value()));
	return std::nullopt;
}

std::optional<std::string> SubProgram::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == false)
		return "A subprogram was missing a name!";
	Name(name.as_string());
	// get the return type. it's under type. if type
	// doesn't exist, return type is void
	std::optional<std::shared_ptr<Typed>> returnType;
	auto type = die.resolve(dwarf::DW_AT::type);
	if (type.valid() == true)
	{
		// parse the return type
		auto parsedType = parser.ParseDIE(type.as_reference());
		if (parsedType.has_value() == false)
			return std::move(parsedType.error());
		if (parsedType.value()->GetType() != Type::Typed)
			return "A subprogram has a non-type return type!";
		m_returnType = std::static_pointer_cast<Typed>(std::move(parsedType.value()));
	}
	// loop through the parameters, which are the sibling's children
	for (const auto param : die)
	{
		if (param.tag != dwarf::DW_TAG::formal_parameter)
			continue;
		auto parsedParam = parser.ParseDIE(param);
		if (parsedParam.has_value() == false)
			return std::move(parsedParam.error());
		if (parsedParam.value()->GetType() != Type::Value)
			return "A subprogram's parameter was a non-value type";
		m_parameters.push_back(std::static_pointer_cast<Value>(
			std::move(parsedParam.value())));
	}
	return std::nullopt;
}

std::optional<std::string> TypeDef::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == false)
		return "A typedef was missing a name!";
	Name(name.as_string());
	// find the type
	auto type = die.resolve(dwarf::DW_AT::type);
	if (type.valid() == false)
		return "A typedef was missing a type!";
	// parse the type
	auto parsedType = parser.ParseDIE(type.as_reference());
	if (parsedType.has_value() == false)
		return std::move(parsedType.error());
	if (parsedType.value()->GetType() != Type::Typed)
		return "A typedef's type was not a type!";
	m_type = std::static_pointer_cast<Typed>(std::move(parsedType.value()));
	return std::nullopt;
}

std::optional<std::string> Value::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == false)
	{
		// this is used for template and function params too,
		// which don't have to have names
		if (die.tag == dwarf::DW_TAG::member)
			return "A value was missing a name!";
	}
	else
		Name(name.as_string());
	// find the type
	auto type = die.resolve(dwarf::DW_AT::type);
	if (type.valid() == false)
		return "A value was missing a type!";
	// parse the type
	auto parsedType = parser.ParseDIE(type.as_reference());
	if (parsedType.has_value() == false)
		return std::move(parsedType.error());
	if (parsedType.value()->GetType() != Type::Typed)
		return "A value's type was not a type!";
	m_type = std::static_pointer_cast<Typed>(std::move(parsedType.value()));
	return std::nullopt;
}

// parser

std::optional<std::string> Parser::ParseDWARF(const dwarf::dwarf& data) noexcept
{
	for (const auto& compilationUnit : data.compilation_units())
	{
		if (auto res = ParseCompilationUnit(compilationUnit); 
			res.has_value() == true)
			return std::move(res.value());
	}
	return std::nullopt;
}

std::optional<std::string> Parser::ParseCompilationUnit(const dwarf::compilation_unit& unit) noexcept
{
	for (const auto& die : unit.root())
	{
		if (auto res = ParseDIE(die); res.has_value() == false)
			return std::move(res.error());
		else
			m_globalNamespace.AddNamed(std::move(res.value()));
	}
	return std::nullopt;
}

tl::expected<std::shared_ptr<Named>, std::string> Parser::ParseDIE(const dwarf::die& die) noexcept
{
	// if we already parsed it, return the entry
	const auto parsedIt = m_parsedEntries.find(die);
	if (parsedIt != m_parsedEntries.end())
		return parsedIt->second;
	std::shared_ptr<Named> result;
	// todo: make a self-registering factory for this
	switch (die.tag)
	{
	case dwarf::DW_TAG::array_type:
		result = std::make_shared<Array>();
		break;
	case dwarf::DW_TAG::base_type:
		result = std::make_shared<BasicType>();
		break;
	case dwarf::DW_TAG::class_type:
	case dwarf::DW_TAG::structure_type:
	case dwarf::DW_TAG::union_type:
		result = std::make_shared<Class>();
		break;
	case dwarf::DW_TAG::const_type:
		result = std::make_shared<ConstType>();
		break;
	case dwarf::DW_TAG::enumeration_type:
		result = std::make_shared<Enum>();
		break;
	case dwarf::DW_TAG::enumerator:
		result = std::make_shared<Enumerator>();
		break;
	case dwarf::DW_TAG::formal_parameter:
	case dwarf::DW_TAG::member:
	case dwarf::DW_TAG::template_type_parameter:
	case dwarf::DW_TAG::template_value_parameter:
		result = std::make_shared<Value>();
		break;
	case dwarf::DW_TAG::imported_declaration:
	case dwarf::DW_TAG::imported_module:
		// we don't care about this
		break;
	case dwarf::DW_TAG::namespace_:
		result = std::make_shared<Namespace>();
		break;
	case dwarf::DW_TAG::pointer_type:
		result = std::make_shared<Pointer>();
		break;
	case dwarf::DW_TAG::ptr_to_member_type:
		result = std::make_shared<PointerToMember>();
		break;
	case dwarf::DW_TAG::reference_type:
		result = std::make_shared<RefType>();
		break;
	case dwarf::DW_TAG::subprogram:
		result = std::make_shared<SubProgram>();
		break;
	case dwarf::DW_TAG::typedef_:
		result = std::make_shared<TypeDef>();
		break;
	default:
		std::cout << "Type dump for " << to_string(die.tag) << ":\n";
		for (const auto& attr : die.attributes())
			std::cout << to_string(attr.first) << ": " << to_string(attr.second) << '\n';
		return tl::make_unexpected("Unimplemented DIE type " + to_string(die.tag));
	}
	m_parsedEntries.emplace(die, result);
	// don't parse empty concepts
	if (result == nullptr)
		return result;
	if (auto parseRes = result->ParseDIE(*this, die);
		parseRes.has_value() == true)
		return tl::make_unexpected(std::move(parseRes.value()));
	return std::move(result);
}