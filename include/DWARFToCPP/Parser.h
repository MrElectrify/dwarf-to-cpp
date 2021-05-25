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
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#if _WIN32
#pragma warning(pop)
#endif

namespace DWARFToCPP
{
	class Parser;

	class Named
	{
	public:
		enum class Type
		{
			Enumerator,
			Imported,
			Namespace,
			SubProgram,
			Typed,
			Value
		};

		virtual ~Named() = default;

		/// @brief Parses a DIE to a named concept
		/// @param parser The parser
		/// @param die The DIE
		/// @return The error, if applicable
		virtual std::optional<std::string> ParseDIE(Parser& parser,
			const dwarf::die& die) noexcept = 0;
	protected:
		/// @param type The basic type of the named concept
		Named(Type type) noexcept :
			m_type(type) {}
	public:
		/// @return The basic type of the named concept
		Type GetType() const noexcept { return m_type; }
		/// @return The name of the concept
		const std::string& GetName() const noexcept { return m_name; }
	protected:
		/// @tparam Str The string type
		/// @param name The name of the concept
		template<typename Str>
		void SetName(Str&& name) noexcept { m_name = std::forward<Str>(name); }
	private:
		Type m_type;
		std::string m_name;
	};

	class Enum;
	class Subroutine;
	class Typed;
	class Value;

	class Enumerator : public Named
	{
	public:
		Enumerator() noexcept : Named(Type::Enumerator) {}

		/// @brief Parses a DIE to a named concept
		/// @param parser The parser
		/// @param die The DIE
		/// @return The error, if applicable
		virtual std::optional<std::string> ParseDIE(Parser& parser,
			const dwarf::die& die) noexcept;
	private:
		uint64_t m_value = 0;
	};

	class Imported : public Named
	{
	public:
		Imported() noexcept : Named(Type::Imported) {}

		/// @brief Parses a DIE to a named concept
		/// @param parser The parser
		/// @param die The DIE
		/// @return The error, if applicable
		virtual std::optional<std::string> ParseDIE(Parser& parser,
			const dwarf::die& die) noexcept;
	};

	class Namespace : public Named
	{
	public:
		/// @brief Creates an empty-named namespace
		Namespace() noexcept : Named(Named::Type::Namespace) {}

		/// @brief Adds a named concept to the namespace
		/// @param named The named concept
		void AddNamed(const std::shared_ptr<Named>& named) noexcept;

		/// @brief Parses a DIE to a named concept
		/// @param parser The parser
		/// @param die The DIE
		/// @return The error, if applicable
		virtual std::optional<std::string> ParseDIE(Parser& parser,
			const dwarf::die& die) noexcept;
	private:
		std::unordered_map<std::string, std::weak_ptr<Named>> m_namedConcepts;
	};

	class SubProgram : public Named
	{
	public:
		SubProgram() noexcept : Named(Type::SubProgram) {}

		/// @brief Parses a DIE to a named concept
		/// @param parser The parser
		/// @param die The DIE
		/// @return The error, if applicable
		virtual std::optional<std::string> ParseDIE(Parser& parser,
			const dwarf::die& die) noexcept;
	private:
		std::optional<std::weak_ptr<Typed>> m_returnType;
		std::vector<std::weak_ptr<Value>> m_parameters;
	};

	class Typed : public Named
	{
	public:
		enum class TypeCode
		{
			Array,
			Basic,
			Class,
			ConstType,
			Enum,
			NamedType,
			Pointer,
			PointerToMember,
			RefType,
			Subroutine,
			TypeDef,
			VolatileType
		};

		/// @param typeCode The typed concept's type code
		Typed(TypeCode typeCode) noexcept : 
			Named(Type::Typed), m_typeCode(typeCode) {}

		/// @return The type code of the typed concept
		TypeCode GetTypeCode() const noexcept { return m_typeCode; }
	private:
		TypeCode m_typeCode;
	};

	class Array : public Typed
	{
	public:
		Array() noexcept : Typed(TypeCode::Array) {}
		
		/// @brief Parses a DIE to a named concept
		/// @param parser The parser
		/// @param die The DIE
		/// @return The error, if applicable
		virtual std::optional<std::string> ParseDIE(Parser& parser,
			const dwarf::die& die) noexcept;

		/// @return The size of the array, in elements
		size_t Size() const noexcept { return m_size; }
		/// @return The type of the array
		const std::weak_ptr<Typed>& Type() const noexcept { return m_type; }
	private:
		size_t m_size = 0;
		std::weak_ptr<Typed> m_type;
	};

	class BasicType : public Typed
	{
	public:
		BasicType() noexcept : Typed(TypeCode::Basic) {}

		/// @brief Parses a DIE to a named concept
		/// @param parser The parser
		/// @param die The DIE
		/// @return The error, if applicable
		virtual std::optional<std::string> ParseDIE(Parser& parser,
			const dwarf::die& die) noexcept;
	};

	class Class : public Typed
	{
	public:
		enum class Accessibility
		{
			Public = 1,
			Protected,
			Private
		};

		Class() noexcept : Typed(TypeCode::Class) {}

		/// @brief Parses a DIE to a named concept
		/// @param parser The parser
		/// @param die The DIE
		/// @return The error, if applicable
		virtual std::optional<std::string> ParseDIE(Parser& parser,
			const dwarf::die& die) noexcept;
	protected:
		dwarf::DW_TAG m_classType{};
		std::vector<std::pair<std::weak_ptr<Named>, Accessibility>> m_members;
		std::vector<std::pair<std::weak_ptr<Class>, Accessibility>> m_parentClasses;
		std::vector<std::weak_ptr<Value>> m_templateParameters;
	};

	class ConstType : public Typed
	{
	public:
		ConstType() noexcept : Typed(TypeCode::ConstType) {}

		/// @brief Parses a DIE to a named concept
		/// @param parser The parser
		/// @param die The DIE
		/// @return The error, if applicable
		virtual std::optional<std::string> ParseDIE(Parser& parser,
			const dwarf::die& die) noexcept;
	private:
		std::weak_ptr<Named> m_type;
	};

	class Enum : public Typed
	{
	public:
		Enum() noexcept : Typed(TypeCode::Enum) {}

		/// @brief Parses a DIE to a named concept
		/// @param parser The parser
		/// @param die The DIE
		/// @return The error, if applicable
		virtual std::optional<std::string> ParseDIE(Parser& parser,
			const dwarf::die& die) noexcept;
	private:
		std::vector<std::weak_ptr<Enumerator>> m_enumerators;
	};

	class NamedType : public Typed
	{
	public:
		NamedType() noexcept : Typed(TypeCode::NamedType) {}

		/// @brief Parses a DIE to a named concept
		/// @param parser The parser
		/// @param die The DIE
		/// @return The error, if applicable
		virtual std::optional<std::string> ParseDIE(Parser& parser,
			const dwarf::die& die) noexcept;
	private:
		std::optional<std::weak_ptr<Typed>> m_type;
	};

	class Pointer : public Typed
	{
	public:
		Pointer() noexcept : Typed(TypeCode::Pointer) {}

		/// @brief Parses a DIE to a named concept
		/// @param parser The parser
		/// @param die The DIE
		/// @return The error, if applicable
		virtual std::optional<std::string> ParseDIE(Parser& parser,
			const dwarf::die& die) noexcept;
	private:
		std::weak_ptr<Named> m_type;
	};

	class PointerToMember : public Typed
	{
	public:
		PointerToMember() noexcept : Typed(TypeCode::PointerToMember) {}

		/// @brief Parses a DIE to a named concept
		/// @param parser The parser
		/// @param die The DIE
		/// @return The error, if applicable
		virtual std::optional<std::string> ParseDIE(Parser& parser,
			const dwarf::die& die) noexcept;
	private:
		std::weak_ptr<Class> m_containingType;
		std::weak_ptr<Subroutine> m_functionType;
	};

	class RefType : public Typed
	{
	public:
		RefType() noexcept : Typed(TypeCode::RefType) {}

		/// @brief Parses a DIE to a named concept
		/// @param parser The parser
		/// @param die The DIE
		/// @return The error, if applicable
		virtual std::optional<std::string> ParseDIE(Parser& parser,
			const dwarf::die& die) noexcept;
	private:
		std::weak_ptr<Named> m_type;
	};

	/// @brief Subroutine is like a subprogram,
	/// only it's just a type and not an actual instance
	class Subroutine : public Typed
	{
	public:
		Subroutine() noexcept : Typed(TypeCode::Subroutine) {}

		/// @brief Parses a DIE to a named concept
		/// @param parser The parser
		/// @param die The DIE
		/// @return The error, if applicable
		virtual std::optional<std::string> ParseDIE(Parser& parser,
			const dwarf::die& die) noexcept;
	private:
		std::optional<std::weak_ptr<Typed>> m_returnType;
		std::vector<std::weak_ptr<Value>> m_parameters;
	};

	class TypeDef : public Typed
	{
	public:
		TypeDef() noexcept : Typed(TypeCode::TypeDef) {}

		/// @brief Parses a DIE to a named concept
		/// @param parser The parser
		/// @param die The DIE
		/// @return The error, if applicable
		virtual std::optional<std::string> ParseDIE(Parser& parser,
			const dwarf::die& die) noexcept;
	private:
		std::weak_ptr<Typed> m_type;
	};

	class VolatileType : public Typed
	{
	public:
		VolatileType() noexcept : Typed(TypeCode::VolatileType) {}

		/// @brief Parses a DIE to a named concept
		/// @param parser The parser
		/// @param die The DIE
		/// @return The error, if applicable
		virtual std::optional<std::string> ParseDIE(Parser& parser,
			const dwarf::die& die) noexcept;
	private:
		std::weak_ptr<Named> m_type;
	};

	class Value : public Named
	{
	public:
		Value() noexcept : Named(Type::Value) {}

		/// @brief Parses a DIE to a named concept
		/// @param parser The parser
		/// @param die The DIE
		/// @return The error, if applicable
		virtual std::optional<std::string> ParseDIE(Parser& parser,
			const dwarf::die& die) noexcept;
	private:
		/// @tparam Str The string type
		/// @param type The type of the value
		/// @param name The name of the value
		template<typename Str>
		Value(std::shared_ptr<Typed> type, Str&& name) noexcept :
			Named(Named::Type::Value, std::forward<Str>(name)),
			m_type(std::move(type)) {}

		std::weak_ptr<Typed> m_type;
	};

	class Parser
	{
	public:
		/// @brief Parses the global namespace from parsed DWARF data,
		/// and stores all classes, namespaces, and instances from the data
		/// @param data The parsed DWARF data
		/// @return The error, if one occurs
		std::optional<std::string> ParseDWARF(const dwarf::dwarf& data) noexcept;

		/// @return The global namespace
		const Namespace& GlobalNamespace() const noexcept { return m_globalNamespace; }
	private:
		// friend each type so they can parse on their own
		// which may require additional parsing from the parser
		friend Array;
		friend BasicType;
		friend Class;
		friend ConstType;
		friend Enum;
		friend NamedType;
		friend Namespace;
		friend Pointer;
		friend PointerToMember;
		friend SubProgram;
		friend Subroutine;
		friend RefType;
		friend TypeDef;
		friend Value;
		friend VolatileType;

		/// @brief Parses a single compliation unit
		/// @param unit The compilation unit to parse
		/// @return The error, if one occurs
		std::optional<std::string> ParseCompilationUnit(const dwarf::compilation_unit& unit) noexcept;
		/// @brief Parses a DIE
		/// @param die The DIE
		/// @return The parsed named concept from the DIE
		tl::expected<std::shared_ptr<Named>, std::string> ParseDIE(const dwarf::die& die) noexcept;

		// the global namespace will contain every type and function found
		Namespace m_globalNamespace;
		// we also store parsed entries here
		std::unordered_map<dwarf::die, std::shared_ptr<Named>> m_parsedEntries;
	};
}

#endif