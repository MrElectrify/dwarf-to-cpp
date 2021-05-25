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
		return tl::make_unexpected(parsedType.error());
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
	std::shared_ptr<Class> result(new Class(name.as_string()));
	// a namespace contains many children. parse each one
	for (const auto& child : die)
	{
		// parse the child
		const auto parsedChild = parser.ParseDie(child);
		if (parsedChild.has_value() == false)
			return tl::make_unexpected(parsedChild.error());
	}
	return result;

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
		const auto parsedChild = parser.ParseDie(child);
		if (parsedChild.has_value() == false)
			return tl::make_unexpected(parsedChild.error());
	}
	return result;
}

tl::expected<std::shared_ptr<Struct>, std::string> Struct::FromDIE(
	Parser& parser, const dwarf::die& die) noexcept
{
	auto class_ = Class::FromDIE(parser, die);
	if (class_.has_value() == false)
		return tl::make_unexpected(class_.error());
	return std::shared_ptr<Struct>(new Struct(std::move(*class_.value())));
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
		return tl::make_unexpected(parsedType.error());
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
		if (const auto res = ParseCompilationUnit(compilationUnit);
			res.has_value() == true)
			return res.value();
	}
	return std::nullopt;
}

std::optional<std::string> Parser::ParseCompilationUnit(const dwarf::compilation_unit& unit) noexcept
{
	for (const auto& die : unit.root())
	{
		if (const auto res = ParseDie(die);
			res.has_value() == false)
			return res.error();
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
			return tl::make_unexpected(array_.error());
		result = std::move(array_.value());
		break;
	}
	case dwarf::DW_TAG::base_type:
	{
		auto type = Type::FromDIE(die);
		if (type.has_value() == false)
			return tl::make_unexpected(type.error());
		result = std::move(type.value());
		break;
	}
	case dwarf::DW_TAG::class_type:
	{
		auto class_ = Class::FromDIE(*this, die);
		if (class_.has_value() == false)
			return tl::make_unexpected(class_.error());
		result = std::move(class_.value());
		break;
	}
	case dwarf::DW_TAG::member:
	{
		auto value = Value::FromDIE(*this, die);
		if (value.has_value() == false)
			return tl::make_unexpected(value.error());
		result = std::move(value.value());
		break;
	}
	case dwarf::DW_TAG::namespace_:
	{
		auto namespace_ = Namespace::FromDIE(*this, die);
		if (namespace_.has_value() == false)
			return tl::make_unexpected(namespace_.error());
		result = std::move(namespace_.value());
		break;
	}
	case dwarf::DW_TAG::structure_type:
	{
		auto struct_ = Struct::FromDIE(*this, die);
		if (struct_.has_value() == false)
			return tl::make_unexpected(struct_.error());
		result = std::move(struct_.value());
		break;
	}
	default:
		return tl::make_unexpected("Unimplemented DIE type " + to_string(die.tag));
	}
	m_parsedEntries.emplace(die, result);
	return result;
}