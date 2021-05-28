#ifndef DWARFTOCPP_PARSER_H_
#define DWARFTOCPP_PARSER_H_

/// @file
/// DWARF Type Parser
/// 5/24/21 15:50

// libelfin includes
#if _WIN32
#pragma warning(push, 0)
#endif
#include <elf++.hh>
#include <dwarf++.hh>

// expected includes
#include <tl/expected.hpp>

// STL includes
#include <memory>
#include <optional>
#include <string>

#if _WIN32
#pragma warning(pop)
#endif

namespace DWARFToCPP
{
	class Parser;

	/// @brief An abstract concept that exists in a language
	class LanguageConcept
	{
	public:
		enum class ConceptType
		{
			Instance,
			Namespace,
			Type,
		};

		/// @param conceptType The type of the language concept
		LanguageConcept(ConceptType conceptType) noexcept : 
			m_conceptType(conceptType) {}
		virtual ~LanguageConcept() noexcept {}

		/// @return The type of the concept
		ConceptType GetConceptType() const noexcept { return m_conceptType; }

		/// @brief Parses a concept from a debug information entry
		virtual std::optional<std::string> Parse(Parser& parser, const dwarf::die& entry) noexcept = 0;
		/// @brief Prints the full concept's C equivalent out to a stream
		/// @param out The output stream
		/// @param indentLevel The indention level
		virtual void Print(std::ostream& out, size_t indentLevel) const noexcept = 0;
	private:
		ConceptType m_conceptType;
	};
	
	/// @brief A data type
	class Type : public LanguageConcept
	{
	public:
		enum class TypeCode
		{
			
		};

		/// @param typeCode The typed concept's type code
		Type(TypeCode typeCode) noexcept :
			LanguageConcept(ConceptType::Type), m_typeCode(typeCode) {}

		/// @return The type code of the typed concept
		TypeCode GetTypeCode() const noexcept { return m_typeCode; }
	private:
		TypeCode m_typeCode;
	};

	/// @brief A data type which has a unique name
	class NamedType : public Type
	{
	public:
		/// @param typeCode The typed concept's type code
		NamedType(Type::TypeCode typeCode) noexcept :
			Type(typeCode) {}

		/// @return The name of the type
		const std::string& GetTypeName() const noexcept { return m_typeName; }
	private:
		/// @brief Sets the type's name
		/// @tparam Str The type of the string
		/// @param typeName The name of the type
		template<typename Str>
		void TypeName(Str&& typeName)
		{
			m_typeName = std::forward<Str>(typeName);
		}

		std::string m_typeName;
	};

	/// @brief A modifier modifies an underlying type,
	/// and is usually represented by an associated keyword
	/// or symbol
	class Modifier : public Type
	{
	public:
		/// @param underlyingType The underlying type being modified
		Modifier(Type::TypeCode underlyingType) noexcept :
			Type(underlyingType) {}
	};

	/// @brief An instance of a certain type
	class Instance : public LanguageConcept
	{
	public:
		Instance() noexcept : LanguageConcept(ConceptType::Instance) {}

		/// @return The name of the instance
		const std::string& GetInstanceName() const noexcept { return m_instanceName; }
		/// @return The type of the instance
		const std::weak_ptr<Type>& GetInstanceType() const noexcept { return m_instanceType; }
	private:
		std::string m_instanceName;
		std::weak_ptr<Type> m_instanceType;
	};

	/// @brief A namespace contains all types and instances
	/// in the global scope
	class Namespace : public LanguageConcept
	{
	public:
		Namespace() noexcept : LanguageConcept(ConceptType::Namespace) {}
	private:
	};

	class Parser
	{
	public:
		/// @brief Parses a single compliation unit
		/// @param unit The compilation unit to parse
		/// @return The error, if one occurs
		std::optional<std::string> Parse(const dwarf::compilation_unit& unit) noexcept;
		/// @brief Parses a language concept from a debug information entry
		/// @param die The entry
		/// @return The parsed language concept from the DIE
		tl::expected<std::shared_ptr<LanguageConcept>, std::string> Parse(const dwarf::die& entry) noexcept;
		/// @brief Parses the global namespace from parsed DWARF data,
		/// and stores all classes, namespaces, and instances from the data
		/// @param data The parsed DWARF data
		/// @return The error, if one occurs
		std::optional<std::string> Parse(const dwarf::dwarf& data) noexcept;

		/// @brief Prints all classes and namespaces to a stream
		/// @param out The output stream
		void Print(std::ostream& out) noexcept;
	private:
		std::unordered_map<const void*, std::shared_ptr<LanguageConcept>> m_parsedConcepts;
	};
}

#endif