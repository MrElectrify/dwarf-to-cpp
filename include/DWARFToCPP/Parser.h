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
		enum class BasicType
		{
			Namespace,
			SubProgram,
			Type,
			Value
		};
	protected:
		/// @tparam Str The string type
		/// @param basicType The basic type of the named concept
		/// @param name The name of the concept
		template<typename Str>
		Named(BasicType basicType, Str&& name) noexcept :
			m_basicType(basicType), m_name(std::forward<Str>(name)) {}
	public:
		/// @return The basic type of the named concept
		BasicType GetBasicType() const noexcept { return m_basicType; }
		/// @return The name of the concept
		const std::string& Name() const noexcept { return m_name; }
	private:
		void Name(std::string name) noexcept { m_name = std::move(name); }

		BasicType m_basicType;
		std::string m_name;
	};

	class Type : public Named
	{
	public:
		enum class TypeCode
		{
			Basic,
			Class
		};

		/// @brief Creates a type from a DIE entry
		/// @param die The DIE entry
		/// @return The type, or the error
		static tl::expected<std::shared_ptr<Type>, std::string> FromDIE(
			const dwarf::die& die) noexcept;
	private:
		friend class Class;

		/// @tparam Str The string type
		/// @param name The name's type
		template<typename Str>
		Type(TypeCode typeCode, Str&& name) noexcept : 
			Named(Named::BasicType::Type, std::forward<Str>(name)),
			m_typeCode(typeCode) {}

		TypeCode m_typeCode;
	};

	class Value : public Named
	{
	public:
		/// @brief Creates a value from a DIE entry. Parses any
		/// types and other named concepts within
		/// @param parser The parser
		/// @param die The DIE entry
		/// @return The value, or the error
		static tl::expected<std::shared_ptr<Value>, std::string> FromDIE(
			Parser& parser, const dwarf::die& die) noexcept;
	private:
		/// @tparam Str The string type
		/// @param type The type of the value
		/// @param name The name of the value
		template<typename Str>
		Value(std::weak_ptr<Type> type, Str&& name) noexcept :
			Named(Named::BasicType::Value, std::forward<Str>(name)),
			m_type(std::move(type)) {}

		std::weak_ptr<Type> m_type;
	};

	class Array : public Type
	{
	public:
		/// @brief Creates an array type from a DIE entry
		/// @param parser The parser
		/// @param die The DIE entry
		/// @return The array type, or the error
		static tl::expected<std::shared_ptr<Array>, std::string> FromDIE(
			Parser& parser, const dwarf::die& die) noexcept;

		/// @return The size of the array, in elements
		size_t Size() const noexcept { return m_size; }
	private:
		/// @param size The array's size
		/// @param baseType The base type of the array
		Array(size_t size, Type baseType) noexcept :
			Type(std::move(baseType)), m_size(size) {}

		size_t m_size;
	};

	class Class : public Type
	{
	public:
		/// @brief Creates a class from a DIE entry. Parses any
		/// types and other named concepts within
		/// @param parser The parser
		/// @param die The DIE entry
		/// @return The class, or the error
		static tl::expected<std::shared_ptr<Class>, std::string> FromDIE(
			Parser& parser, const dwarf::die& die) noexcept;
	protected:
		/// @tparam Str The string type
		/// @param struct_ Whether or not the class is considered a struct
		/// @param name The name
		template<typename Str>
		Class(bool struct_, Str&& name) noexcept : Type(
			Type::TypeCode::Class, std::forward<Str>(name)),
			m_struct(struct_) {}

		bool m_struct;
		std::vector<std::shared_ptr<Named>> m_members;
		std::vector<std::weak_ptr<Class>> m_parentClasses;
	};

	class SubProgram : public Named
	{
	public:
		/// @brief Creates a subprogram from a DIE entry
		/// @param parser The parser
		/// @param die The DIE entry
		/// @return The subprogram, or the error
		static tl::expected<std::shared_ptr<SubProgram>, std::string> FromDIE(
			Parser& parser, const dwarf::die& die) noexcept;
	private:
		/// @tparam Str The string type
		/// @param returnType The return type of the subprogram
		/// @param name The name of the subprogram
		template<typename Str>
		SubProgram(std::optional<std::weak_ptr<Type>> returnType, Str&& name) noexcept :
			Named(Named::BasicType::Value, std::forward<Str>(name)),
			m_returnType(std::move(returnType)) {}

		std::optional<std::weak_ptr<Type>> m_returnType;
		std::vector<std::shared_ptr<Value>> m_parameters;
	};

	class Namespace : public Named
	{
	public:
		/// @brief Creates an empty-named namespace
		Namespace() noexcept : Named(Named::BasicType::Namespace, "") {}

		/// @brief Creates a namespace from a DIE entry. Parses any
		/// types and other named concepts within
		/// @param parser The parser
		/// @param die The DIE entry
		/// @return The type, or the error
		static tl::expected<std::shared_ptr<Namespace>, std::string> FromDIE(
			Parser& parser, const dwarf::die& die) noexcept;

		/// @brief Adds a named concept to the namespace
		/// @param named The named concept
		void AddNamed(std::shared_ptr<Named> named) noexcept;
	private:
		/// @tparam Str The string type
		/// @param name The name's type
		template<typename Str>
		Namespace(Str&& name) noexcept : Named(
			Named::BasicType::Namespace, std::forward<Str>(name)) {}

		std::unordered_map<std::string, std::shared_ptr<Named>> m_namedConcepts;
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
		friend Class;
		friend Namespace;
		friend SubProgram;
		friend Type;
		friend Value;

		/// @brief Parses a single compliation unit
		/// @param unit The compilation unit to parse
		/// @return The error, if one occurs
		std::optional<std::string> ParseCompilationUnit(const dwarf::compilation_unit& unit) noexcept;
		/// @brief Parses a DIE
		/// @param die The DIE
		/// @return The parsed named concept from the DIE
		tl::expected<std::shared_ptr<Named>, std::string> ParseDie(const dwarf::die& die) noexcept;

		// the global namespace will contain every type and function found
		Namespace m_globalNamespace;
		// we also store parsed entries here
		std::unordered_map<dwarf::die, std::shared_ptr<Named>> m_parsedEntries;
	};
}

#endif