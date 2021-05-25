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
	if (parsedType->get()->GetType() != Type::Typed)
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
		std::static_pointer_cast<Typed>(std::move(parsedType.value()))));
}

tl::expected<std::shared_ptr<Class>, std::string> Class::FromDIE(
	Parser& parser, const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	std::string className;
	if (name.valid() == true)
		className = name.as_string();
	bool publicDefault = (die.tag != dwarf::DW_TAG::class_type);
	std::shared_ptr<Class> result(new Class(die.tag, std::move(className)));
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
				return tl::make_unexpected("An class inheritance did not have a type!");
			auto parsedInheritanceType = parser.ParseDie(inheritanceType.as_reference());
			if (parsedInheritanceType.has_value() == false)
				return tl::make_unexpected(std::move(parsedInheritanceType.error()));
			if (parsedInheritanceType.value()->GetType() != Type::Typed)
				return tl::make_unexpected("A class inheritance was not a type!");
			auto parentClass = std::static_pointer_cast<Typed>(std::move(parsedInheritanceType.value()));
			// ensure it is also a class
			if (parentClass->GetTypeCode() != TypeCode::Class)
				return tl::make_unexpected("A class inheritance was not a class!");
			result->m_parentClasses.emplace_back(std::static_pointer_cast<Class>(
				std::move(parentClass)), accessibility);
			continue;
		}
		// the child is a type. parse it
		auto parsedChild = parser.ParseDie(child);
		if (parsedChild.has_value() == false)
			return tl::make_unexpected(std::move(parsedChild.error()));
		// make sure the type is not a namespace
		if (parsedChild.value()->GetType() == Type::Namespace)
			return tl::make_unexpected("A class had a nested namespace!");
		result->m_members.emplace_back(std::move(parsedChild.value()), accessibility);
	}
	return result;
}

tl::expected<std::shared_ptr<Enum>, std::string> Enum::FromDIE(
	Parser& parser, const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == false)
		return tl::make_unexpected("An enum was missing a name!");
	std::shared_ptr<Enum> result(new Enum(name.as_string()));
	// parse the enumerators
	for (auto child : die)
	{
		auto enumerator = parser.ParseDie(child);
		if (enumerator.has_value() == false)
			return tl::make_unexpected(std::move(enumerator.error()));
		if (enumerator.value()->GetType() != Type::Enumerator)
			return tl::make_unexpected("An enum had a non-enumerator child!");
		result->m_enumerators.push_back(std::static_pointer_cast<Enumerator>(
			std::move(enumerator.value())));
	}
	return nullptr;
}

tl::expected<std::shared_ptr<Enumerator>, std::string> Enumerator::FromDIE(
	const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == false)
		return tl::make_unexpected("An enumerator was missing a name!");
	auto value = die.resolve(dwarf::DW_AT::const_value);
	if (value.valid() == false)
		return tl::make_unexpected("An enumerator was missing a value!");
	return std::shared_ptr<Enumerator>(new Enumerator(value.as_uconstant(), name.as_string()));
}

void Namespace::AddNamed(std::shared_ptr<Named> named) noexcept
{
	if (named == nullptr)
		return;
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

tl::expected<std::shared_ptr<Pointer>, std::string> Pointer::FromDIE(
	Parser& parser, const dwarf::die& die) noexcept
{
	auto type = die.resolve(dwarf::DW_AT::type);
	if (type.valid() == false)
		return tl::make_unexpected("A pointer was missing a type!");
	auto parsedType = parser.ParseDie(type.as_reference());
	if (parsedType.has_value() == false)
		return tl::make_unexpected(std::move(parsedType.error()));
	return std::shared_ptr<Pointer>(new Pointer(
		std::static_pointer_cast<Typed>(std::move(parsedType.value()))));
}

tl::expected<std::shared_ptr<PointerToMember>, std::string> PointerToMember::FromDIE(
	Parser& parser, const dwarf::die& die) noexcept
{
	auto containingType = die.resolve(dwarf::DW_AT::containing_type);
	if (containingType.valid() == false)
		return tl::make_unexpected("A pointer-to-member was missing a containing type!");
	auto parsedContainingNamed = parser.ParseDie(containingType.as_reference());
	if (parsedContainingNamed.has_value() == false)
		return tl::make_unexpected(std::move(parsedContainingNamed.error()));
	if (parsedContainingNamed.value()->GetType() != Type::Typed)
		return tl::make_unexpected("A pointer-to-member had a non-typed containing type!");
	auto parsedContainingType = std::static_pointer_cast<Typed>(std::move(parsedContainingNamed.value()));
	if (parsedContainingType->GetTypeCode() != TypeCode::Class)
		return tl::make_unexpected("A pointer-to-member's containing type was not class-based!");
	auto functionType = die.resolve(dwarf::DW_AT::type);
	if (functionType.valid() == false)
		return tl::make_unexpected("A pointer-to-member was missing a function type!");
	auto parsedFunctionType = parser.ParseDie(functionType.as_reference());
	if (parsedFunctionType.has_value() == false)
		return tl::make_unexpected(std::move(parsedFunctionType.error()));
	if (parsedFunctionType.value()->GetType() != Type::SubProgram)
		return tl::make_unexpected("A pointer-to-member had a non-type function!");
	return std::shared_ptr<PointerToMember>(new PointerToMember(
		std::static_pointer_cast<Class>(std::move(parsedContainingType)),
		std::static_pointer_cast<SubProgram>(std::move(parsedFunctionType.value()))));
}

tl::expected<std::shared_ptr<SubProgram>, std::string> SubProgram::FromDIE(
	Parser& parser, const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == false)
		return tl::make_unexpected("A subprogram was missing a name!");
	// get the return type. it's under type. if type
	// doesn't exist, return type is void
	std::optional<std::shared_ptr<Typed>> returnType;
	auto type = die.resolve(dwarf::DW_AT::type);
	if (type.valid() == true)
	{
		// parse the return type
		auto parsedType = parser.ParseDie(type.as_reference());
		if (parsedType.has_value() == false)
			return tl::make_unexpected(std::move(parsedType.error()));
		if (parsedType.value()->GetType() != Type::Typed)
			return tl::make_unexpected("A subprogram has a non-type return type!");
		returnType = std::static_pointer_cast<Typed>(std::move(parsedType.value()));
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

tl::expected<std::shared_ptr<BasicType>, std::string> 
BasicType::FromDIE(const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == false)
		return tl::make_unexpected("A basic type was missing a name!");
	return std::shared_ptr<BasicType>(new BasicType(name.as_string()));
}

tl::expected<std::shared_ptr<TypeDef>, std::string> TypeDef::FromDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == false)
		return tl::make_unexpected("A typedef was missing a name!");
	// find the type
	auto type = die.resolve(dwarf::DW_AT::type);
	if (type.valid() == false)
		return tl::make_unexpected("A typedef was missing a type!");
	// parse the type
	auto parsedType = parser.ParseDie(type.as_reference());
	if (parsedType.has_value() == false)
		return tl::make_unexpected(std::move(parsedType.error()));
	if (parsedType.value()->GetType() != Type::Typed)
		return tl::make_unexpected("A typedef's type was not a type!");
	return std::shared_ptr<TypeDef>(new TypeDef(
		std::static_pointer_cast<Typed>(std::move(parsedType.value())), 
		name.as_string()));
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
	if (parsedType.value()->GetType() != Type::Typed)
		return tl::make_unexpected("A value's type was not a type!");
	return std::shared_ptr<Value>(new Value(
		std::static_pointer_cast<Typed>(std::move(parsedType.value())), 
		name.as_string()));
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
	// todo: make a self-registering factory for this
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
		auto basicType = BasicType::FromDIE(die);
		if (basicType.has_value() == false)
			return tl::make_unexpected(std::move(basicType.error()));
		result = std::move(basicType.value());
		break;
	}
	case dwarf::DW_TAG::class_type:
	case dwarf::DW_TAG::structure_type:
	case dwarf::DW_TAG::union_type:
	{
		auto class_ = Class::FromDIE(*this, die);
		if (class_.has_value() == false)
			return tl::make_unexpected(std::move(class_.error()));
		result = std::move(class_.value());
		break;
	}
	case dwarf::DW_TAG::enumeration_type:
	{
		auto enum_ = Enum::FromDIE(*this, die);
		if (enum_.has_value() == false)
			return tl::make_unexpected(std::move(enum_.error()));
		result = std::move(enum_.value());
		break;
	}
	case dwarf::DW_TAG::enumerator:
	{
		auto enumerator = Enumerator::FromDIE(die);
		if (enumerator.has_value() == false)
			return tl::make_unexpected(std::move(enumerator.error()));
		result = std::move(enumerator.value());
		break;
	}
	case dwarf::DW_TAG::imported_declaration:
	case dwarf::DW_TAG::imported_module:
		// we don't care about this
		break;
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
	case dwarf::DW_TAG::pointer_type:
	{
		auto pointer = Pointer::FromDIE(*this, die);
		if (pointer.has_value() == false)
			return tl::make_unexpected(std::move(pointer.error()));
		result = std::move(pointer.value());
		break;
	}
	case dwarf::DW_TAG::ptr_to_member_type:
	{
		auto pointerToMember = PointerToMember::FromDIE(*this, die);
		if (pointerToMember.has_value() == false)
			return tl::make_unexpected(std::move(pointerToMember.error()));
		result = std::move(pointerToMember.value());
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
	case dwarf::DW_TAG::typedef_:
	{
		auto typedef_ = TypeDef::FromDIE(*this, die);
		if (typedef_.has_value() == false)
			return tl::make_unexpected(std::move(typedef_.error()));
		result = std::move(typedef_.value());
		break;
	}
	default:
	{
		std::cout << "Type dump for " << to_string(die.tag) << ":\n";
		for (const auto& attr : die.attributes())
			std::cout << to_string(attr.first) << ": " << to_string(attr.second) << '\n';
		return tl::make_unexpected("Unimplemented DIE type " + to_string(die.tag));
	}
	}
	m_parsedEntries.emplace(die, result);
	return result;
}