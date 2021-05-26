#include <DWARFToCPP/Parser.h>

#include <ranges>
#include <stack>
#include <unordered_set>

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
	m_size = size.as_uconstant() + 1;
	SetName(m_type.lock()->GetName() + '[' + std::to_string(m_size) + ']');
	return std::nullopt;
}

void Array::PrintToFile(std::ofstream& outFile, size_t indentLevel) noexcept
{

}

std::optional<std::string> BasicType::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == false)
		return "A basic type was missing a name!";
	SetName(name.as_string());
	return std::nullopt;
}

void BasicType::PrintToFile(std::ofstream& outFile, size_t indentLevel) noexcept
{

}

std::optional<std::string> Class::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	m_classType = die.tag;
	auto name = die.resolve(dwarf::DW_AT::name);
	std::string className;
	if (name.valid() == true)
		SetName(name.as_string());
	else
		SetName(std::to_string(std::hash<void*>()(this)));
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
			// it is guaranteed to be a named type
			m_templateParameters.push_back(std::static_pointer_cast<Value>(
				std::move(parsedChild.value())));
			continue;
		}
		// it's a normal member. add the relationship and store the member
		parser.AddParent(*parsedChild.value(), *this);
		m_members.emplace_back(parsedChild.value(), accessibility);
	}
	return std::nullopt;
}

void Class::PrintToFile(std::ofstream& outFile, size_t indentLevel) noexcept
{
	PrintIndents(outFile, indentLevel);
	outFile << ToString(m_classType) << ' ' << GetName() << ' ';
	if (m_parentClasses.empty() == false)
	{
		outFile << ": ";
		for (auto it = m_parentClasses.begin(); it != m_parentClasses.end(); ++it)
		{
			if (it != m_parentClasses.begin())
				outFile << ", ";
			outFile << ToString(it->second) << ' ';
			outFile << it->first.lock()->GetName();
		}
	}
	outFile << '\n';
	PrintIndents(outFile, indentLevel);
	outFile << "{\n";
	// print each member
	Accessibility lastAccessibility = (m_classType == dwarf::DW_TAG::class_type) ?
		Accessibility::Private : Accessibility::Public;
	for (const auto& memberPair : m_members)
	{
		if (memberPair.second != lastAccessibility)
		{
			PrintIndents(outFile, indentLevel);
			outFile << ToString(memberPair.second) << ":\n";
			lastAccessibility = memberPair.second;
		}
		memberPair.first.lock()->PrintToFile(outFile, indentLevel + 1);
	}
	PrintIndents(outFile, indentLevel);
	outFile << "};\n";
}

std::string Class::ToString(Accessibility accessibility) noexcept
{
	switch (accessibility)
	{
	case Accessibility::Public:
		return "public";
	case Accessibility::Protected:
		return "protected";
	case Accessibility::Private:
		return "private";
	}
	return "";
}

std::string Class::ToString(dwarf::DW_TAG classType) noexcept
{
	switch (classType)
	{
	case dwarf::DW_TAG::class_type:
		return "class";
	case dwarf::DW_TAG::structure_type:
		return "struct";
	case dwarf::DW_TAG::union_type:
		return "union";
	}
	return "";
}

std::optional<std::string> ConstType::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	// parse the embedded type
	auto type = die.resolve(dwarf::DW_AT::type);
	if (type.valid() == true)
	{
		auto parsedType = parser.ParseDIE(type.as_reference());
		if (parsedType.has_value() == false)
			return std::move(parsedType.error());
		if (parsedType.value()->GetType() != Type::Typed)
			return "A const type was not a type!";
		m_type = std::static_pointer_cast<Typed>(std::move(parsedType.value()));
	}
	SetName("const " + (m_type.has_value() == true ? m_type->lock()->GetName() : "void"));
	return std::nullopt;
}

void ConstType::PrintToFile(std::ofstream& outFile, size_t indentLevel) noexcept
{

}

std::optional<std::string> Enum::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	// enums dont have to have names
	if (name.valid() == true)
		SetName(name.as_string());
	else
		SetName(std::to_string(std::hash<void*>()(this)));
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

void Enum::PrintToFile(std::ofstream& outFile, size_t indentLevel) noexcept
{

}

std::optional<std::string> Enumerator::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == false)
		return "An enumerator was missing a name!";
	SetName(name.as_string());
	auto value = die.resolve(dwarf::DW_AT::const_value);
	if (value.valid() == false)
		return "An enumerator was missing a value!";
	if (value.get_type() == dwarf::value::type::sconstant)
		m_value = value.as_sconstant();
	else if (value.get_type() == dwarf::value::type::uconstant ||
		value.get_type() == dwarf::value::type::constant)
		m_value = value.as_uconstant();
	else
		return "An enumerator had an invalid type!";
	return std::nullopt;
}

void Enumerator::PrintToFile(std::ofstream& outFile, size_t indentLevel) noexcept
{

}

std::optional<std::string> Ignored::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	// we don't care about anything here
	return std::nullopt;
}

void Ignored::PrintToFile(std::ofstream& outFile, size_t indentLevel) noexcept
{

}

void Named::PrintIndents(std::ofstream& outFile, size_t indentLevel) noexcept
{
	for (size_t i = 0; i < indentLevel; ++i)
		outFile << '\t';
}

std::optional<std::string> NamedType::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	// the name is actually kinda misleading. it may or
	// may not be named
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == true)
		SetName(name.as_string());
	// it does, however, have a base type
	auto type = die.resolve(dwarf::DW_AT::type);
	if (type.valid() == false)
		return "A named type did not have a type!";
	auto parsedType = parser.ParseDIE(type.as_reference());
	if (parsedType.has_value() == false)
		return std::move(parsedType.error());
	if (parsedType.value()->GetType() != Type::Typed)
		return "A named type's type was not a type!";
	m_type = std::static_pointer_cast<Typed>(std::move(parsedType.value()));
	return std::nullopt;
}

void NamedType::PrintToFile(std::ofstream& outFile, size_t indentLevel) noexcept
{

}

std::optional<std::string> Namespace::AddNamed(Parser& parser, std::shared_ptr<Named> named) noexcept
{
	if (named == nullptr)
		return std::nullopt;
	const auto& name = named->GetName();
	// just ignore empty names
	if (name.empty() == true)
		return std::nullopt;
	// see if it already exists
	const auto conceptIt = m_namedConcepts.find(name);
	if (conceptIt == m_namedConcepts.end())
	{
		// add the relationship if this is not the global namespace
		if (GetName().empty() == false)
			parser.AddParent(*named, *this);
		m_namedConcepts.emplace(name, std::move(named));
		return std::nullopt;
	}
	// if it's not a namespace, it's likely just included by multiple files
	if (named->GetType() != Type::Namespace)
		return std::nullopt;
	// lock the existing concept and append the new list
	auto existingConcept = conceptIt->second.lock();
	if (named->GetType() != existingConcept->GetType())
		return "Symbol " + name + " in namespace " + GetName() + " type mismatch";
	auto existingNamespace = std::static_pointer_cast<Namespace>(std::move(existingConcept));
	auto newNamespace = std::static_pointer_cast<Namespace>(std::move(named));
	existingNamespace->m_namedConcepts.insert(newNamespace->m_namedConcepts.begin(), 
		newNamespace->m_namedConcepts.end());
	return std::nullopt;
}

std::optional<std::shared_ptr<const Named>> Namespace::GetNamedConcept(
	const std::string& name) const noexcept
{
	const auto conceptIt = m_namedConcepts.find(name);
	if (conceptIt == m_namedConcepts.end())
		return std::nullopt;
	return conceptIt->second.lock();
}

std::optional<std::string> Namespace::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == true)
		SetName(name.as_string());
	else
		SetName("::");
	// a namespace contains many children. parse each one
	for (const auto& child : die)
	{
		// parse the child
		auto parsedChild = parser.ParseDIE(child);
		if (parsedChild.has_value() == false)
			return std::move(parsedChild.error());
		auto error = AddNamed(parser, parsedChild.value());
		if (error.has_value() == true)
			return std::move(error);
	}
	return std::nullopt;
}

void Namespace::PrintToFile(std::ofstream& outFile, size_t indentLevel) noexcept
{
	const bool global = (GetName().empty() == true);
	if (global == false)
	{
		PrintIndents(outFile, indentLevel);
		outFile << "namespace " << GetName() << "\n";
		PrintIndents(outFile, indentLevel);
		outFile << "{\n";
	}
	for (const auto& namedPair : m_namedConcepts)
	{
		const auto namedConcept = namedPair.second.lock();
		if (namedConcept->GetType() != Type::Namespace)
		{
			if (namedConcept->GetType() == Type::Typed)
			{
				// make sure it is a class type
				if (std::static_pointer_cast<Typed>(namedConcept)->GetTypeCode() !=
					Typed::TypeCode::Class)
					continue;
			}
			else
				continue;
		}
		namedConcept->PrintToFile(outFile, indentLevel + 1 - global);
	}
	if (global == false)
	{
		PrintIndents(outFile, indentLevel);
		outFile << "};\n";
	}
}

std::optional<std::string> Pointer::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	auto type = die.resolve(dwarf::DW_AT::type);
	if (type.valid() == true)
	{
		auto parsedType = parser.ParseDIE(type.as_reference());
		if (parsedType.has_value() == false)
			return std::move(parsedType.error());
		if (parsedType.value()->GetType() != Type::Typed)
			return "A pointer was not in reference to a type!";
		m_type = std::static_pointer_cast<Typed>(std::move(parsedType.value()));
	}
	SetName((m_type.has_value() == true ? m_type->lock()->GetName() : "void") + '*');
	return std::nullopt;
}

void Pointer::PrintToFile(std::ofstream& outFile, size_t indentLevel) noexcept
{

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
	auto parsedContainingType = std::static_pointer_cast<Typed>(
		std::move(parsedContainingNamed.value()));
	if (parsedContainingType->GetTypeCode() != TypeCode::Class)
		return "A pointer-to-member's containing type was not class-based!";
	m_containingType = std::static_pointer_cast<Class>(std::move(parsedContainingType));
	auto functionType = die.resolve(dwarf::DW_AT::type);
	if (functionType.valid() == false)
		return "A pointer-to-member was missing a function type!";
	auto parsedFunctionNamed = parser.ParseDIE(functionType.as_reference());
	if (parsedFunctionNamed.has_value() == false)
		return std::move(parsedFunctionNamed.error());
	if (parsedFunctionNamed.value()->GetType() != Type::Typed)
		return "A pointer-to-member had a non-type function!";
	auto parsedFunctionType = std::static_pointer_cast<Typed>(
		std::move(parsedFunctionNamed.value()));
	if (parsedFunctionType->GetTypeCode() != TypeCode::Subroutine)
		return "A pointer-to-member had a non-subroutine function!";
	m_functionType = std::static_pointer_cast<Subroutine>(std::move(parsedFunctionType));
	// todo: construct a pointer-to-member type
	return std::nullopt;
}

void PointerToMember::PrintToFile(std::ofstream& outFile, size_t indentLevel) noexcept
{

}

std::optional<std::string> RefType::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	// parse the embedded type
	auto type = die.resolve(dwarf::DW_AT::type);
	if (type.valid() == false)
		return "A ref type did not have a type!";
	auto parsedType = parser.ParseDIE(type.as_reference());
	if (parsedType.has_value() == false)
		return std::move(parsedType.error());
	if (parsedType.value()->GetType() != Type::Typed)
		return "A ref type was not a type!";
	m_type = std::static_pointer_cast<Typed>(std::move(parsedType.value()));
	SetName(m_type.lock()->GetName() + '&');
	return std::nullopt;
}

void RefType::PrintToFile(std::ofstream& outFile, size_t indentLevel) noexcept
{

}

std::optional<std::string> RRefType::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	// parse the embedded type
	auto type = die.resolve(dwarf::DW_AT::type);
	if (type.valid() == false)
		return "A rref type did not have a type!";
	auto parsedType = parser.ParseDIE(type.as_reference());
	if (parsedType.has_value() == false)
		return std::move(parsedType.error());
	if (parsedType.value()->GetType() != Type::Typed)
		return "A rref type was not a type!";
	m_type = std::static_pointer_cast<Typed>(std::move(parsedType.value()));
	SetName(m_type.lock()->GetName() + "&&");
	return std::nullopt;
}

void RRefType::PrintToFile(std::ofstream& outFile, size_t indentLevel) noexcept
{

}

std::optional<std::string> SubProgram::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	// see if this is a specification
	auto spec = die.resolve(dwarf::DW_AT::specification);
	if (spec.valid() == true)
	{
		// this is a specification. find the existing function and add params
		auto existingNamed = parser.ParseDIE(spec.as_reference());
		if (existingNamed.has_value() == false)
			return std::move(existingNamed.error());
		if (existingNamed.value()->GetType() != Type::SubProgram)
			return "A subprogram specification was not a subprogram!";
		auto existingFn = std::static_pointer_cast<SubProgram>(std::move(existingNamed.value()));
		// erase existing parameters and rewrite them
		existingFn->m_parameters.clear();
		for (const auto param : die)
		{
			if (param.tag != dwarf::DW_TAG::formal_parameter)
				continue;
			auto parsedParam = parser.ParseDIE(param);
			if (parsedParam.has_value() == false)
				return std::move(parsedParam.error());
			if (parsedParam.value()->GetType() != Type::Value)
				return "A subprogram's parameter was a non value-type";
			existingFn->m_parameters.push_back(std::static_pointer_cast<Value>(
				std::move(parsedParam.value())));
		}
		// leave ourselves empty
		return std::nullopt;
	}
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == false)
		return "A subprogram was missing a name!";
	SetName(name.as_string());
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
	// see if we are virtual
	auto virtuality = die.resolve(dwarf::DW_AT::virtuality);
	if (virtuality.valid() == true && virtuality.as_uconstant() == 1)
		m_virtual = true;
	// loop through the parameters, which are the sibling's children
	for (const auto param : die)
	{
		if (param.tag != dwarf::DW_TAG::formal_parameter)
			continue;
		auto artificial = param.resolve(dwarf::DW_AT::artificial);
		auto parsedParam = parser.ParseDIE(param);
		if (parsedParam.has_value() == false)
			return std::move(parsedParam.error());
		if (parsedParam.value()->GetType() != Type::Value)
			return "A subprogram's parameter was a non value-type";
		m_parameters.push_back(std::static_pointer_cast<Value>(
			std::move(parsedParam.value())));
	}
	return std::nullopt;
}

void SubProgram::PrintToFile(std::ofstream& outFile, size_t indentLevel) noexcept
{
	PrintIndents(outFile, indentLevel);
	// if we are virtual, print that
	if (m_virtual == true)
		outFile << "virtual ";
	if (m_returnType.has_value() == true)
		outFile << m_returnType->lock()->GetName();
	else
		outFile << "void";
	outFile << ' ' << GetName() << '(';
	for (auto paramIt = m_parameters.begin(); paramIt != m_parameters.end(); ++paramIt)
	{
		if (paramIt != m_parameters.begin())
			outFile << ", ";
		auto param = paramIt->lock();
		outFile << param->GetValueType().lock()->GetName();
		if (param->GetName().empty() == false)
			outFile << ' ' << param->GetName();
	}
	outFile << ");\n";
}

std::optional<std::string> Subroutine::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	auto returnType = die.resolve(dwarf::DW_AT::type);
	if (returnType.valid() == true)
	{
		// parse the type
		auto parsedType = parser.ParseDIE(returnType.as_reference());
		if (parsedType.has_value() == false)
			return std::move(parsedType.error());
		if (parsedType.value()->GetType() != Type::Typed)
			return "A subroutine's return type was not a type!";
		m_returnType = std::static_pointer_cast<Typed>(std::move(parsedType.value()));
	}
	std::string name = "FunctionPtr<" + ((m_returnType.has_value() == true) ?
		m_returnType->lock()->GetName() : "void");
	name += '(';
	// parse each parameter
	for (auto paramIt = die.begin(); paramIt != die.end(); ++paramIt)
	{
		if (paramIt->tag != dwarf::DW_TAG::formal_parameter)
			continue;
		if (paramIt != die.begin())
			name += ", ";
		auto parsedParam = parser.ParseDIE(*paramIt);
		if (parsedParam.has_value() == false)
			return std::move(parsedParam.error());
		if (parsedParam.value()->GetType() != Type::Value)
			return "A subroutine had a non-value parameter";
		m_parameters.emplace_back(std::static_pointer_cast<Value>(
			std::move(parsedParam.value())));
	}
	name += ")>";
	SetName(std::move(name));
	return std::nullopt;
}

void Subroutine::PrintToFile(std::ofstream& outFile, size_t indentLevel) noexcept
{

}

std::optional<std::string> TypeDef::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	auto name = die.resolve(dwarf::DW_AT::name);
	if (name.valid() == false)
		return "A typedef was missing a name!";
	SetName(name.as_string());
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

void TypeDef::PrintToFile(std::ofstream& outFile, size_t indentLevel) noexcept
{
	PrintIndents(outFile, indentLevel);
	outFile << "typedef " << m_type.lock()->GetName() << ' ' << GetName() << ";\n";
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
		SetName(name.as_string());
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

void Value::PrintToFile(std::ofstream& outFile, size_t indentLevel) noexcept
{
	PrintIndents(outFile, indentLevel);
	outFile << m_type.lock()->GetName() << ' ' << GetName() << ";\n";
}

std::optional<std::string> VolatileType::ParseDIE(Parser& parser,
	const dwarf::die& die) noexcept
{
	// parse the embedded type
	auto type = die.resolve(dwarf::DW_AT::type);
	if (type.valid() == false)
		return "A volatile type did not have a type!";
	auto parsedType = parser.ParseDIE(type.as_reference());
	if (parsedType.has_value() == false)
		return std::move(parsedType.error());
	if (parsedType.value()->GetType() != Type::Typed)
		return "A volatile type was not a type!";
	m_type = std::static_pointer_cast<Typed>(std::move(parsedType.value()));
	SetName(m_type.lock()->GetName() + '&');
	return std::nullopt;
}

void VolatileType::PrintToFile(std::ofstream& outFile, size_t indentLevel) noexcept
{

}

// parser

void Parser::AddParent(const Named& child, const Named& parent) noexcept
{
	m_childToParentMap.emplace(&child, &parent);
}

std::optional<std::string> Parser::ParseDWARF(const dwarf::dwarf& data) noexcept
{
	size_t unitNo = 1;
	for (const auto& compilationUnit : data.compilation_units())
	{
		size_t startingTypes = m_parsedEntries.size();
		if (auto res = ParseCompilationUnit(compilationUnit); 
			res.has_value() == true)
			return std::move(res.value());
		size_t currentTypes = m_parsedEntries.size();
		size_t deltaTypes = currentTypes - startingTypes;
		printf("Parsed unit %zd/%zd with %zd new types and %zd total\n",
			unitNo++, data.compilation_units().size(), deltaTypes, m_parsedEntries.size());
	}
	return std::nullopt;
}

std::optional<std::string> Parser::ParseCompilationUnit(const dwarf::compilation_unit& unit) noexcept
{
	for (const auto& die : unit.root())
	{
		if (auto res = ParseDIE(die); res.has_value() == false)
			return std::move(res.error());
		else if (auto error = m_globalNamespace.AddNamed(*this, std::move(res.value()));
			error.has_value() == true)
			return std::move(error);
	}
	return std::nullopt;
}

tl::expected<std::shared_ptr<Named>, std::string> Parser::ParseDIE(const dwarf::die& die) noexcept
{
	// if we already parsed it, return the entry. use unit and offset to save space
	const auto parsedIt = m_parsedEntries.find(
		reinterpret_cast<const char*>(
		&die.get_unit()) + die.get_section_offset());
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
	case dwarf::DW_TAG::variable:
		result = std::make_shared<Value>();
		break;
	case dwarf::DW_TAG::imported_declaration:
	case dwarf::DW_TAG::imported_module:
	case static_cast<dwarf::DW_TAG>(0x4106):
		result = std::make_shared<Ignored>();
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
	case dwarf::DW_TAG::rvalue_reference_type:
		result = std::make_shared<RRefType>();
		break;
	case dwarf::DW_TAG::subprogram:
		result = std::make_shared<SubProgram>();
		break;
	case dwarf::DW_TAG::subroutine_type:
		result = std::make_shared<Subroutine>();
		break;
	case dwarf::DW_TAG::template_type_parameter:
	case dwarf::DW_TAG::template_value_parameter:
		result = std::make_shared<NamedType>();
		break;
	case dwarf::DW_TAG::typedef_:
		result = std::make_shared<TypeDef>();
		break;
	case dwarf::DW_TAG::volatile_type:
		result = std::make_shared<VolatileType>();
		break;
	default:
		return tl::make_unexpected("Unimplemented DIE type " + to_string(die.tag));
	}
	m_parsedEntries.emplace(reinterpret_cast<const char*>(
			&die.get_unit()) + die.get_section_offset(), result);
	if (auto parseRes = result->ParseDIE(*this, die);
		parseRes.has_value() == true)
		return tl::make_unexpected(std::move(parseRes.value()));
	return std::move(result);
}

void Parser::PrintToFile(std::ofstream& outFile) noexcept
{
	// print the global namespace
	m_globalNamespace.PrintToFile(outFile);
}