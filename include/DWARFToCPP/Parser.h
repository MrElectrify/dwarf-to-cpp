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

		/// @return Whether or not the concept has an explicit name
		virtual bool IsNamed() const noexcept { return false; }
		/// @brief Parses a concept from a debug information entry
		virtual std::optional<std::string> Parse(Parser& parser, const dwarf::die& entry) noexcept = 0;
		/// @brief Prints the full concept's C equivalent out to a stream
		/// @param out The output stream
		/// @param indentLevel The indention level
		virtual void Print(std::ostream& out, size_t indentLevel) const noexcept = 0;
	private:
		ConceptType m_conceptType;
	};
	
	/// @brief A concept that has a unique name
	class NamedConcept : public LanguageConcept
	{
	public:
		/// @param conceptType The type of the language concept
		NamedConcept(LanguageConcept::ConceptType conceptType) noexcept :
			LanguageConcept(conceptType) {}

		/// @return The name of the concept
		const std::string& GetName() const noexcept { return m_name; }
		/// @return Whether or not the concept has an explicit name
		virtual bool IsNamed() const noexcept { return true; }
	protected:
		/// @brief Parses the name from a debug info entry
		/// @param entry The debug info entry
		/// @return The error, if one occurs
		virtual std::optional<std::string> Parse(Parser& parser, const dwarf::die& entry) noexcept;
	private:
		std::string m_name;
	};

	/// @brief A map of NamedConcepts, referenced by their name
	class NamedConceptMap
	{
	public:
		/// @brief Attempts to add a language concept to the map. If
		/// the type is not named, fail
		/// @param languageConcept The language concept
		/// @return Whether or not the type was added
		bool AddConcept(std::shared_ptr<LanguageConcept> languageConcept) noexcept;
		/// @brief Adds a named concept to the map
		/// @param namedConcept The named concept
		void AddConcept(const std::shared_ptr<NamedConcept>& namedConcept) noexcept;
	private:
		std::unordered_map<std::string, std::weak_ptr<NamedConcept>> m_namedConcepts;
	};

	/// @brief A data type
	class Type : public LanguageConcept
	{
	public:
		enum class TypeCode
		{
			Pointer
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
	class NamedType : public Type, public NamedConcept
	{
	public:
		/// @param typeCode The typed concept's type code
		NamedType(Type::TypeCode typeCode) noexcept :
			Type(typeCode), NamedConcept(ConceptType::Type) {}
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

		/// @return The referenced type that is modified
		const std::weak_ptr<Type> GetReferencedType() const noexcept { return m_referencedType; }

		/// @brief Parses the name from a debug info entry
		/// @param entry The debug info entry
		/// @return The error, if one occurs
		virtual std::optional<std::string> Parse(Parser& parser, const dwarf::die& entry) noexcept;
	private:
		std::weak_ptr<Type> m_referencedType;
	};

	/// @brief A memory pointer to a specific type
	class Pointer : public Modifier
	{
	public:
		Pointer() noexcept : Modifier(Type::TypeCode::Pointer) {}

		/// @brief Prints the full concept's C equivalent out to a stream
		/// @param out The output stream
		/// @param indentLevel The indention level
		virtual void Print(std::ostream& out, size_t indentLevel) const noexcept;
	};

	/// @brief An instance of a certain type
	class Instance : public NamedConcept
	{
	public:
		Instance() noexcept : NamedConcept(ConceptType::Instance) {}

		/// @return The type of the instance
		const std::weak_ptr<Type>& GetInstanceType() const noexcept { return m_instanceType; }
	private:
		std::weak_ptr<Type> m_instanceType;
	};

	/// @brief A namespace contains all types and instances
	/// in the global scope
	class Namespace : public NamedConcept, public NamedConceptMap
	{
	public:
		Namespace() noexcept : NamedConcept(ConceptType::Namespace) {}

		/// @brief Parses a concept from a debug information entry
		virtual std::optional<std::string> Parse(Parser& parser, const dwarf::die& entry) noexcept;
		/// @brief Prints the full concept's C equivalent out to a stream
		/// @param out The output stream
		/// @param indentLevel The indention level
		virtual void Print(std::ostream& out, size_t indentLevel) const noexcept;
	};

	/// @brief Parser parses DWARF entries into suitable
	/// data structures for later output
	class Parser : public NamedConceptMap
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