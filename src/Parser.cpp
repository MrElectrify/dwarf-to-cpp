#include <DWARFToCPP/Parser.h>

#include <ranges>
#include <stack>
#include <unordered_set>

#include <iostream>

using namespace DWARFToCPP;

// types

tl::expected<std::shared_ptr<Array>, std::string> Array::FromDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	// find the type
	auto type = die.resolve(dwarf::DW_AT::type);
	if (type.valid() == false)
		return tl::make_unexpected("An array was missing a type!");
	// parse the type
	auto parsedType = parser.ParseDie(type.as_reference());
	if (parsedType.has_value() == false)
		return tl::make_unexpected(std::move(parsedType.error()));
	if (parsedType->get()->GetBasicType() != BasicType::Type)
		return tl::make_unexpected("An array's type was not a type!");
	// get the child, which contains size
	auto child = *die.begin();
	if (child.tag != dwarf::DW_TAG::subrange_type)
		return tl::make_unexpected("An array was missing its subrange info!");
	// parse the size
	auto size = child.resolve(dwarf::DW_AT::upper_bound);
	if (size.valid() == false)
		return tl::make_unexpected("An array's subrange info was missing the size!");
	// the subrange size + 1 is the array's size
	return std::shared_ptr<Array>(new Array(size.as_uconstant() + 1,
		*std::static_pointer_cast<Type>(parsedType.value())));
}

tl::expected<std::shared_ptr<Class>, std::string> Class::FromDIE(
	Parser& parser, const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == false)
		return tl::make_unexpected("A class was missing a name!");
	bool struct_ = (die.tag == dwarf::DW_TAG::structure_type);
	std::shared_ptr<Class> result(new Class(struct_, name.as_string()));
	// a namespace contains many children. parse each one
	for (const auto& child : die)
	{
		// parse the child
		auto parsedChild = parser.ParseDie(child);
		if (parsedChild.has_value() == false)
			return tl::make_unexpected(std::move(parsedChild.error()));
		// make sure the type is not a namespace
		if (parsedChild.value()->GetBasicType() == BasicType::Namespace)
			return tl::make_unexpected("A class had a nested namespace!");
		result->m_members.push_back(std::move(parsedChild.value()));
	}
	return result;

}

void Namespace::AddNamed(std::shared_ptr<Named> named) noexcept
{
	auto name = named->Name();
	m_namedConcepts.emplace(std::move(name), std::move(named));
}

tl::expected<std::shared_ptr<Namespace>, std::string> Namespace::FromDIE(
	Parser& parser, const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == false)
		return tl::make_unexpected("A namespace was missing a name!");
	std::shared_ptr<Namespace> result(new Namespace(name.as_string()));
	// a namespace contains many children. parse each one
	for (const auto& child : die)
	{
		// parse the child
		auto parsedChild = parser.ParseDie(child);
		if (parsedChild.has_value() == false)
			return tl::make_unexpected(std::move(parsedChild.error()));
		auto childName = parsedChild.value()->Name();
		result->m_namedConcepts.emplace(std::move(childName), std::move(parsedChild.value()));
	}
	return result;
}

tl::expected<std::shared_ptr<SubProgram>, std::string> SubProgram::FromDIE(
	Parser& parser, const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == false)
		return tl::make_unexpected("A subprogram was missing a name!");
	// get the return type. it's under type. if type
	// doesn't exist, return type is void
	std::optional<std::weak_ptr<Type>> returnType;
	auto type = die.resolve(dwarf::DW_AT::type);
	if (type.valid() == true)
	{
		// parse the return type
		auto parsedType = parser.ParseDie(type.as_reference());
		if (parsedType.has_value() == false)
			return tl::make_unexpected(std::move(parsedType.error()));
		if (parsedType.value()->GetBasicType() != BasicType::Type)
			return tl::make_unexpected("A subprogram has a non-type return type!");
		returnType = std::static_pointer_cast<Type>(parsedType.value());
	}
	std::shared_ptr<SubProgram> result(new SubProgram(std::move(returnType), name.as_string()));
	// loop through the parameters, which are the sibling's children
	for (const auto param : die)
	{
		if (param.tag != dwarf::DW_TAG::formal_parameter)
			continue;
		// parse the parameter as a Value, because it has the same entries
		auto value = Value::FromDIE(parser, param);
		if (value.has_value() == false)
			return tl::make_unexpected(std::move(value.error()));
		result->m_parameters.push_back(std::move(value.value()));
	}
	return result;
}

tl::expected<std::shared_ptr<Type>, std::string> Type::FromDIE(const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == false)
		return tl::make_unexpected("A basic type was missing a name!");
	return std::shared_ptr<Type>(new Type(TypeCode::Basic, name.as_string()));
}

tl::expected<std::shared_ptr<Value>, std::string> Value::FromDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == false)
		return tl::make_unexpected("A value was missing a name!");
	// find the type
	auto type = die.resolve(dwarf::DW_AT::type);
	if (type.valid() == false)
		return tl::make_unexpected("A value was missing a type!");
	// parse the type
	auto parsedType = parser.ParseDie(type.as_reference());
	if (parsedType.has_value() == false)
		return tl::make_unexpected(std::move(parsedType.error()));
	if (parsedType.value()->GetBasicType() != BasicType::Type)
		return tl::make_unexpected("A value's type was not a type!");
	return std::shared_ptr<Value>(new Value(
		std::static_pointer_cast<Type>(parsedType.value()), name.as_string()));
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
		if (auto res = ParseDie(die); res.has_value() == false)
			return std::move(res.error());
		else
			m_globalNamespace.AddNamed(std::move(res.value()));
	}
	return std::nullopt;
}

tl::expected<std::shared_ptr<Named>, std::string> Parser::ParseDie(const dwarf::die& die) noexcept
{
	// if we already parsed it, return the entry
	const auto parsedIt = m_parsedEntries.find(die);
	if (parsedIt != m_parsedEntries.end())
		return parsedIt->second;
	std::shared_ptr<Named> result;
	switch (die.tag)
	{
	case dwarf::DW_TAG::array_type:
	{
		auto array_ = Array::FromDIE(*this, die);
		if (array_.has_value() == false)
			return tl::make_unexpected(std::move(array_.error()));
		result = std::move(array_.value());
		break;
	}
	case dwarf::DW_TAG::base_type:
	{
		auto type = Type::FromDIE(die);
		if (type.has_value() == false)
			return tl::make_unexpected(std::move(type.error()));
		result = std::move(type.value());
		break;
	}
	case dwarf::DW_TAG::class_type:
	{
		auto class_ = Class::FromDIE(*this, die);
		if (class_.has_value() == false)
			return tl::make_unexpected(std::move(class_.error()));
		result = std::move(class_.value());
		break;
	}
	case dwarf::DW_TAG::member:
	{
		auto value = Value::FromDIE(*this, die);
		if (value.has_value() == false)
			return tl::make_unexpected(std::move(value.error()));
		result = std::move(value.value());
		break;
	}
	case dwarf::DW_TAG::namespace_:
	{
		auto namespace_ = Namespace::FromDIE(*this, die);
		if (namespace_.has_value() == false)
			return tl::make_unexpected(std::move(namespace_.error()));
		result = std::move(namespace_.value());
		break;
	}
	case dwarf::DW_TAG::structure_type:
	{
		auto struct_ = Class::FromDIE(*this, die);
		if (struct_.has_value() == false)
			return tl::make_unexpected(std::move(struct_.error()));
		result = std::move(struct_.value());
		break;
	}
	case dwarf::DW_TAG::subprogram:
	{
		auto subprogram = SubProgram::FromDIE(*this, die);
		if (subprogram.has_value() == false)
			return tl::make_unexpected(std::move(subprogram.error()));
		result = std::move(subprogram.value());
		break;
	}
	default:
		return tl::make_unexpected("Unimplemented DIE type " + to_string(die.tag));
	}
	m_parsedEntries.emplace(die, result);
	return result;
}