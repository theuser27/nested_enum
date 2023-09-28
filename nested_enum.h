#include <cstdint>
#include <array>
#include <string_view>
#include <type_traits>
#include <numeric>
#include <optional>
#include <functional>
#include <cassert>

namespace nested_enum
{
    template <size_t N>
    struct fixed_string
    {
        using char_type = std::string_view::value_type;

        constexpr fixed_string(const char_type(&array)[N + 1]) noexcept
        {
            std::copy(std::begin(array), std::end(array), data_.begin());
        }

        constexpr explicit fixed_string(std::string_view string) noexcept : 
            fixed_string{ string, std::make_index_sequence<N>{} }
        {
            assert(string.size() == N);
        }

        constexpr const char_type *data() const noexcept { return data_.data(); }
        static constexpr size_t size() noexcept { return N; }

        constexpr operator std::string_view() const noexcept { return { data(), size() }; }

        std::array<char_type, N + 1> data_;

    private:
        template <size_t... Indices>
        constexpr fixed_string(std::string_view str, std::index_sequence<Indices...>) noexcept :
            data_{str[Indices]..., static_cast<char_type>('\0')} { }
    };

    template <size_t N>
    fixed_string(const char(&)[N])->fixed_string<N - 1>;

	namespace detail
	{
		template<typename Derived, typename Base>
		concept DerivedOrIs = std::is_base_of_v<Base, Derived> || std::is_same_v<Base, Derived>;

		template<typename T>
		concept Enum = std::is_enum_v<T>;

		template<typename T>
		concept Integral = std::is_integral_v<T>;

		// checks if a type is complete
		template <class T, class = void>
		struct is_complete_type : std::false_type { };

		template <class T>
		struct is_complete_type<T, decltype(void(sizeof(T)))> : std::true_type { };

		template <class T>
		inline constexpr auto is_complete_type_v = is_complete_type<T>::value;

		// converts an array of integers into an integer_sequence
		template<auto A, class = std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(A)>>>>
		struct as_index_sequence;

		template<auto A, std::size_t ...I>
		struct as_index_sequence<A, std::index_sequence<I...>>
		{
			using type = std::integer_sequence<typename std::decay_t<decltype(A)>::value_type, A[I]...>;
		};

		template<typename T>
		using pure_type_t = std::remove_pointer_t<std::remove_cvref_t<T>>;

		template<typename TupleLike>
		constexpr size_t get_tuple_size()
		{
			return std::tuple_size_v<pure_type_t<TupleLike>>;
		}

        constexpr auto count_if(const auto &container, auto predicate)
        {
            size_t count = 0;
            auto first = container.begin();
            auto last = container.end();
            for (; first != last; ++first)
                if (predicate(*first))
                    ++count;
            return count;
        }

        template<typename T>
        constexpr auto find_index(const auto &container, const T& value)
        {
            auto first = container.begin();
            auto last = container.end();

            for (; first != last; ++first)
                if (*first == value)
                    return std::optional{ (size_t)(first - container.begin()) };
        
            return std::optional<size_t>{};
        }

		template <bool attemptToReturnArray = true, class Callable, class Tuple>
		constexpr decltype(auto) apply_one(Callable &&callable, Tuple &&tuple)
		{
			return[]<size_t ... Indices>(Callable && callable, Tuple && tuple, std::index_sequence<Indices...>) -> decltype(auto)
			{
				if constexpr (std::is_same_v<std::invoke_result_t<Callable, std::tuple_element_t<0, std::remove_cvref_t<Tuple>>>, void>)
					(std::invoke(std::forward<Callable>(callable), std::get<Indices>(std::forward<Tuple>(tuple))), ...);
				else if constexpr (attemptToReturnArray)
					return std::array{ std::invoke(std::forward<Callable>(callable), std::get<Indices>(std::forward<Tuple>(tuple)))... };
				else
					return std::make_tuple(std::invoke(std::forward<Callable>(callable), std::get<Indices>(std::forward<Tuple>(tuple)))...);
			}(std::forward<Callable>(callable), std::forward<Tuple>(tuple),
				std::make_index_sequence<get_tuple_size<decltype(tuple)>()>{});
		}

		// callable must be a functor
		template <auto TupleLike, bool attemptToReturnArray = true, class Callable>
		constexpr decltype(auto) apply_one(Callable &&callable)
		{
			return[]<size_t ... Indices>(Callable && callable, std::index_sequence<Indices...>) -> decltype(auto)
			{
				if constexpr (attemptToReturnArray)
					return std::array{ callable.template operator() < std::get<Indices>(TupleLike) > ()... };
				else
					return std::make_tuple(callable.template operator() < std::get<Indices>(TupleLike) > ()...);
			}(std::forward<Callable>(callable), std::make_index_sequence<get_tuple_size<decltype(TupleLike)>()>{});
		}

		template <auto Predicate, auto &TupleLike>
		constexpr auto filter()
		{
			if constexpr (get_tuple_size<decltype(TupleLike)>() == 0)
				return std::tuple<>{};
			else
			{
				constexpr auto areIncluded = apply_one(Predicate, TupleLike);
				constexpr auto includedIndices = [&]()
				{
					constexpr size_t size = count_if(areIncluded,
						[](bool b) { return b == true; });

					std::array<size_t, size> values{};

					for (size_t i = 0, j = 0; i < areIncluded.size(); ++i)
						if (areIncluded[i])
							values[j++] = i;

					return values;
				}();

				return[]<size_t ... Indices>(std::index_sequence<Indices...>) -> decltype(auto)
				{
					//return std::tuple{ std::get<Indices>(TupleLike)... };
					return std::make_tuple(std::get<Indices>(TupleLike)...);
				}(typename as_index_sequence<includedIndices>::type{});
			}
		}

		template <typename T>
		constexpr auto type_name()
		{
			std::string_view name;
		#if defined(__GNUC__) or defined (__clang__)
			name = __PRETTY_FUNCTION__;
			std::string_view typeBit = "T = ";
			name.remove_prefix(name.find(typeBit) + typeBit.length());
			name.remove_suffix(name.length() - name.rfind(']'));
		#elif defined(_MSC_VER)
			name = __FUNCSIG__;
			name.remove_prefix(name.find('<') + 1);
			std::string_view enumBit = "enum ";
			if (auto enum_token_pos = name.find(enumBit); enum_token_pos != std::string_view::npos)
				name.remove_prefix(enum_token_pos + enumBit.length());
			name.remove_suffix(name.length() - name.rfind(">(void)"));
		#endif

			return name;
		}

		template<auto Value>
		constexpr auto value_name(bool clean = false)
		{
			std::string_view name;
		#if defined(__GNUC__) or defined (__clang__)
			name = __PRETTY_FUNCTION__;
			std::string_view valueBit = "Value = ";
			name.remove_prefix(name.find(valueBit) + valueBit.length());
			name.remove_suffix(name.length() - name.rfind(']'));
		#elif defined(_MSC_VER)
			name = __FUNCSIG__;
			name.remove_prefix(name.find('<') + 1);
			std::string_view enumBit = "enum ";
			if (auto enum_token_pos = name.find(enumBit); enum_token_pos != std::string_view::npos)
				name.remove_prefix(enum_token_pos + enumBit.length());
			name.remove_suffix(name.length() - name.rfind(">(bool)"));
		#else
		    #error Unsupported Compiler
		#endif
            if (clean)
            {
                std::string_view scope = "::";
                name.remove_prefix(name.rfind(scope) + scope.length());
            }
			return name;
		}

		constexpr int8_t getDigit(char character)
		{
			if (character >= '0' && character <= '9')
				return (int8_t)(character - '0');

			if (character >= 'A' && character <= 'Z')
				return (int8_t)(character - 'A' + 10);

			if (character >= 'a' && character <= 'z')
				return (int8_t)(character - 'a' + 10);

			return 0;
		}

		constexpr std::string_view trim_white_space(std::string_view view)
		{
			auto begin = view.find_first_not_of(' ');
			if (begin == std::string::npos)
				return {};

			auto end = view.find_last_not_of(' ');
			auto range = end - begin + 1;

			return view.substr(begin, range);
		}

		template<Integral Int = int64_t>
		constexpr Int get_int_from_string(std::string_view view)
		{
			auto trimmedView = trim_white_space(view);

			bool isNegative = false;
			if (trimmedView.starts_with('-'))
			{
				trimmedView.remove_prefix(1);
				isNegative = true;
			}

			Int number = 0, decimal = 1;
			for (size_t i = trimmedView.size(); i-- > 0; )
			{
				if (trimmedView[i] == '\'')
					continue;
				number += getDigit(trimmedView[i]) * decimal;
				decimal *= 10;
			}

			return (isNegative) ? -number : number;
		}

		template<typename E, fixed_string staticString, bool hasIds = false>
		constexpr auto get_array_of_values()
		{
			using underlying_type = std::underlying_type_t<E>;
			constexpr size_t size = []() -> size_t
			{
				auto view = trim_white_space({ staticString });
				if (view.empty())
					return 0;
				size_t size = count_if(view, [](char c) { return c == ','; }) + 1;
				return (!hasIds) ? size : size / 2;
			}();

			auto view = trim_white_space({ staticString });
			std::array<E, size> enumValues{};

			if constexpr (size != 0)
			{
				underlying_type currentValue = 0;
				size_t index = 0;
				[[maybe_unused]] size_t outerIndex = 0;

				while (!view.empty())
				{
					auto separatorIndex = view.find(',');
					auto substr = view.substr(0, separatorIndex);
					if constexpr (!hasIds)
					{
						if (auto equalsIndex = substr.find('='); equalsIndex != substr.npos)
							currentValue = get_int_from_string(substr.substr(equalsIndex));

						enumValues[index++] = (E)currentValue++;
					}
					else
					{
						if (outerIndex % 2 == 0)
						{
							if (auto equalsIndex = substr.find('='); equalsIndex != substr.npos)
								currentValue = get_int_from_string(substr.substr(equalsIndex));

							enumValues[index++] = (E)currentValue++;
						}
					}

					outerIndex++;
					if (separatorIndex == view.npos)
						break;
					view.remove_prefix(separatorIndex + 1);
				}
			}

			return enumValues;
		}

		// bless their soul https://stackoverflow.com/a/54932626
		namespace tupleMagic
		{
			template <typename T> struct is_tuple : std::false_type { };
			template <typename... Ts> struct is_tuple<std::tuple<Ts...>> : std::true_type { };

			template<typename T>
			constexpr decltype(auto) as_tuple(T t)
			{
				return std::make_tuple(t);
			}

			template<typename ...Ts>
			constexpr decltype(auto) as_tuple(std::tuple<Ts...> t)
			{
				return t;
			}

			template<typename T>
			constexpr decltype(auto) flatten(T t)
			{
				return t;
			}

			template<typename T>
			constexpr decltype(auto) flatten(std::tuple<T> t)
			{
				return flatten(std::get<0>(t));
			}

			template<typename ...Ts, std::enable_if_t<!(is_tuple<Ts>::value || ...), bool> = false>
			constexpr decltype(auto) flatten(std::tuple<Ts...> t)
			{
				return t;
			}

			template<typename ...Ts, std::enable_if_t<(is_tuple<Ts>::value || ...), bool> = false>
			constexpr decltype(auto) flatten(std::tuple<Ts...> t)
			{
				return std::apply([](auto...ts) { return flatten(std::tuple_cat(as_tuple(flatten(ts))...)); }, t);
			}
		}
	}

    enum InnerOuterAll { InnerNodes, OuterNodes, AllNodes };

	template<class E>
	struct nested_enum
	{
		using underlying_enum_type = int64_t;

		template<class>
		friend struct nested_enum;

        constexpr nested_enum() = default;
        constexpr nested_enum(const nested_enum &) = default;
        template<typename Value>
        constexpr nested_enum(const Value &value)
        {
            static_assert(std::is_same_v<typename E::Value, Value>, "Value is not of the same enum type");
            static_cast<E &>(*this).value = value;
        }
        constexpr nested_enum &operator=(const nested_enum &) = default;
        template<typename Value>
        constexpr nested_enum &operator=(const Value &value)
        {
            static_assert(std::is_same_v<typename E::Value, Value>, "Value is not of the same enum type");
            static_cast<E &>(*this).value = value;
            return *this;
        }

		static constexpr auto enum_type_name(bool clean = false) noexcept
		{
			constexpr auto name = detail::type_name<typename E::Value>();
			constexpr auto trimmedName = name.substr(0, name.rfind("::"));
			auto cleanName = trimmedName;
			if (clean)
			{
				std::string_view scope = "::";
				cleanName.remove_prefix(cleanName.rfind(scope) + scope.length());
			}
			return cleanName;
		}
		template<InnerOuterAll Selection = AllNodes>
        static constexpr auto enum_values() noexcept
		{
			if constexpr (E::enumValues.size() == 0 || Selection == AllNodes)
				return E::enumValues;
			else
			{
                constexpr auto valuesNeeded = detail::apply_one([]<typename T>(T &&) -> bool
                    {
                        constexpr bool outerOrInnerNodes = Selection == OuterNodes;
                        return (!detail::is_complete_type_v<typename detail::pure_type_t<T>::type>)
                            == outerOrInnerNodes;
                    },
					E::subtypes_);

                constexpr auto result = [&]()
                {
                    constexpr size_t size = detail::count_if(valuesNeeded,
						[](bool b) { return b == true; });

                    std::array<typename E::Value, size> values;
                    for (size_t i = 0, j = 0; i < valuesNeeded.size(); i++)
                        if (valuesNeeded[i])
                            values[j++] = E::enumValues[i];
                    return values;
                }();
                
                return result;
			}
		}
		static constexpr size_t enum_count(InnerOuterAll selection = AllNodes) noexcept 
        {
            if constexpr (detail::get_tuple_size<decltype(E::subtypes_)>() == 0)
                return E::enumValues.size();
            else
            {
                if (selection == AllNodes)
                    return E::enumValues.size();
                
                auto subtypesSizes = detail::apply_one([&]<typename T>(T &&) -> size_t
					{
						using type = typename detail::pure_type_t<T>::type;
						if ((selection == AllNodes) || (detail::is_complete_type_v<type> && selection == InnerNodes) ||
						    (!detail::is_complete_type_v<type> && selection == OuterNodes))
							return 1;
                        else
                            return 0;
					},
					E::subtypes_);
                
                return std::accumulate(subtypesSizes.begin(), subtypesSizes.end(), (size_t)0);
            }
        }
        template<InnerOuterAll Selection = AllNodes>
        static constexpr auto enum_integers() noexcept
        {
            constexpr auto enumValues = enum_values<Selection>();
            constexpr auto integerArray = [&]()
            {
                std::array<underlying_enum_type, enumValues.size()> result;
                for (size_t i = 0; i < result.size(); i++)
                    result[i] = (underlying_enum_type)enumValues[i];
                
                return result;
            }();
            return integerArray;
        }
        template<InnerOuterAll Selection = AllNodes>
		static constexpr auto enum_ids() noexcept
        {
            if constexpr (E::ids.size() == 0 || Selection == AllNodes)
                return E::ids;
            else
            {
                constexpr auto idsNeeded = detail::apply_one([]<typename T>(T &&) -> bool
                    {
                        constexpr bool outerOrInnerNodes = Selection == OuterNodes;
                        return (!detail::is_complete_type_v<typename detail::pure_type_t<T>::type>)
                            == outerOrInnerNodes;
                    },
					E::subtypes_);

                constexpr auto result = [&]()
                {
                    constexpr size_t size = detail::count_if(idsNeeded,
						[](bool b) { return b == true; });

                    std::array<std::string_view, size> values;
                    for (size_t i = 0, j = 0; i < idsNeeded.size(); i++)
                        if (idsNeeded[i])
                            values[j++] = E::ids[i];
                    return values;
                }();

                return result;
            }
        }
		template<InnerOuterAll Selection = AllNodes>
        static constexpr auto enum_strings(bool clean = false) noexcept
		{
			if constexpr (enum_count(Selection) == 0)
			{
				// if this isn't assigned to a constexpr variable msvc won't recognise the function as constexpr
				constexpr auto enumStrings = std::array<std::string_view, 0>{};
				return enumStrings;
			}
			else
			{
				constexpr auto enumStrings = detail::apply_one<enum_values<Selection>()>([]<auto Value>() { return detail::value_name<Value>(); });
				constexpr auto enumStringsClean = detail::apply_one<enum_values<Selection>()>([]<auto Value>() { return detail::value_name<Value>(true); });
				if (!clean)
                    return enumStrings;
                else
                    return enumStringsClean;
			}
		}
        template<InnerOuterAll Selection = AllNodes>
		static constexpr auto enum_strings_and_ids(bool clean = false) noexcept
		{
            constexpr auto enumIds = enum_ids<Selection>();
			if constexpr (enumIds.size() == 0)
				return std::array<std::pair<std::string_view, std::string_view>, 0>{};
			else
			{
                constexpr auto stringsAndIds = [&]()
                {
                    std::array<std::pair<std::string_view, std::string_view>, enumIds.size()> result{};
				    for (size_t i = 0; i < enumIds.size(); ++i)
					    result[i] = { enum_strings<Selection>(false)[i], enumIds[i] };
                    return result;
                }();

                constexpr auto stringsAndIdsClean = [&]()
                {
                    std::array<std::pair<std::string_view, std::string_view>, enumIds.size()> result{};
				    for (size_t i = 0; i < enumIds.size(); ++i)
					    result[i] = { enum_strings<Selection>(true)[i], enumIds[i] };
                    return result;
                }();

                if (!clean)
				    return stringsAndIds;
                else
                    return stringsAndIdsClean;
			}
		}

    private:
		static constexpr size_t enum_count_recursive_internal(InnerOuterAll selection = AllNodes) noexcept
		{
            size_t count = (selection == InnerNodes) || (selection == AllNodes);
			if constexpr (detail::get_tuple_size<decltype(E::subtypes_)>() == 0)
				return count;
			else
			{
				auto subtypesSizes = detail::apply_one([&]<typename T>(T &&) -> size_t
					{
						using type = typename detail::pure_type_t<T>::type;
						if constexpr (detail::is_complete_type_v<type>)
							return type::enum_count_recursive_internal(selection);
						else
							return (selection == OuterNodes) || (selection == AllNodes);
					},
					E::subtypes_);

				return std::accumulate(subtypesSizes.begin(), subtypesSizes.end(), count);
			}
		}
    public:
        static constexpr size_t enum_count_recursive(InnerOuterAll selection = AllNodes) noexcept
		{
            size_t count = enum_count_recursive_internal(selection);
            if ((selection == InnerNodes) || (selection == AllNodes))
                return --count;
            return count;
        }
	private:
		template<InnerOuterAll Selection = AllNodes>
		static constexpr auto enum_values_recursive_internal() noexcept
		{
			if constexpr (detail::get_tuple_size<decltype(E::subtypes_)>() == 0)
				return std::tuple<>{};
			else
			{
				// checking which of the contained "enums" we need (outer or inner nodes)
				constexpr auto subtypesNeeded = detail::apply_one([]<typename T>(T &&) -> bool
                    {
                        if constexpr (Selection == AllNodes)
                            return true;
                        else
                        {
                            constexpr bool outerOrInnerNodes = Selection == OuterNodes;
                            return (!detail::is_complete_type_v<typename detail::pure_type_t<T>::type>)
                                == outerOrInnerNodes;
                        }
                    },
					E::subtypes_);

				// getting an array of values we need
				constexpr auto values = [&]()
				{
					using EnumType = typename E::Value;

					constexpr size_t size = detail::count_if(subtypesNeeded,
						[](bool b) { return b == true; });
					std::array<EnumType, size> values{};

					for (size_t i = 0, j = 0; i < subtypesNeeded.size(); ++i)
						if (subtypesNeeded[i])
							values[j++] = enum_values<AllNodes>()[i];

					return values;
				}();

				// new tuple of only the inner enum nodes
				constexpr auto subtypesToQuery = detail::filter<[]<typename T>(T &&) -> bool
					{
						using subType = typename detail::pure_type_t<T>::type;

						// does the enum exist
						if constexpr (!detail::is_complete_type_v<subType>)
							return false;
						else
						{
							// does the enum have any values
							if constexpr (detail::get_tuple_size<decltype(subType::subtypes_)>() == 0)
								return false;
							// different branches based on what we want
							else if constexpr ((Selection == AllNodes) || (Selection == OuterNodes))
								return true;
							else
							{
								constexpr auto subSubtypesNeeded = detail::filter<[]<typename U>(U &&) -> bool
									{
										return detail::is_complete_type_v<typename detail::pure_type_t<U>::type>;
									},
									subType::subtypes_ > ();

								return detail::get_tuple_size<decltype(subSubtypesNeeded)>() != 0;
							}
						}
					},
					E::subtypes_>();

				// returning only the values if we don't have any subtypes to query
				if constexpr (detail::get_tuple_size<decltype(subtypesToQuery)>() == 0)
					return std::make_tuple(values);
				else
				{
					constexpr auto subtypesTuples = detail::apply_one<false>([]<typename T>(T &&)
                        {
                            using type = typename detail::pure_type_t<T>::type;
                            return type::template enum_values_recursive_internal<Selection>();
                        },
						subtypesToQuery);

					if constexpr (values.size() == 0)
						return std::make_tuple(subtypesTuples);
					else
						return std::tuple_cat(std::make_tuple(values), subtypesTuples);
				}
			}
		}
	public:
		template<InnerOuterAll Selection = AllNodes, bool flattenTuple = true>
		static constexpr auto enum_values_recursive() noexcept
		{
            if constexpr (flattenTuple)
                return detail::tupleMagic::flatten(enum_values_recursive_internal<Selection>());
            else
                return enum_values_recursive_internal<Selection>();
		}



		static constexpr auto enum_string(detail::Enum auto value, bool clean = false) -> std::optional<std::string_view>
		{
			static_assert(std::is_same_v<decltype(value), typename E::Value>, "Provided value doesn't match enum type");

			constexpr auto values = enum_values<AllNodes>();
			auto index = detail::find_index(values, value);
			if (!index.has_value())
				return {};
            
            return enum_strings<AllNodes>(clean)[index.value()];
		}
        static constexpr auto enum_string_by_id(std::string_view id, bool clean = false) -> std::optional<std::string_view>
		{
			constexpr auto enumIds = enum_ids<AllNodes>();
			auto index = detail::find_index(enumIds, id);
			if (!index.has_value())
				return {};
            
            return enum_strings(clean)[index.value()];
		}
        static constexpr auto enum_id(detail::Enum auto value) -> std::optional<std::string_view>
		{
			static_assert(std::is_same_v<decltype(value), typename E::Value>, "Provided value doesn't match enum type");

            constexpr auto values = enum_values<AllNodes>();
			auto index = detail::find_index(values, value);
            if (!index.has_value())
				return {};
            
            return enum_ids<AllNodes>()[index.value()];
		}
        static constexpr auto enum_id(std::string_view enumString) -> std::optional<std::string_view>
		{
            constexpr auto strings = enum_strings<AllNodes>();
			auto index = detail::find_index(strings, enumString);
			if (!index.has_value())
				return {};
            
            return enum_ids<AllNodes>()[index.value()];
		}
		static constexpr auto enum_string_and_id(detail::Enum auto value, bool clean = false) -> std::optional<std::pair<std::string_view, std::string_view>>
		{
			static_assert(std::is_same_v<decltype(value), typename E::Value>, "Provided value doesn't match enum type");

			constexpr auto values = enum_values<AllNodes>();
			auto index = detail::find_index(values, value);
			if (!index.has_value())
				return {};

            auto stringsAndIds = enum_strings_and_ids<AllNodes>(clean);
			if (index.value() >= stringsAndIds.size())
                return {};
            
			return stringsAndIds[index.value()];
		}
		static constexpr auto enum_integer(detail::Enum auto value)
		{
			static_assert(std::is_same_v<decltype(value), typename E::Value>, "Provided value doesn't match enum type");
			return (underlying_enum_type)value;
		}
		static constexpr auto enum_integer(std::string_view enumString) -> std::optional<underlying_enum_type>
		{
			constexpr auto enumStrings = enum_strings<AllNodes>(false);
			auto index = detail::find_index(enumStrings, enumString);
			if (!index.has_value())
				return {};

			return (underlying_enum_type)enum_values<AllNodes>()[index.value()];
		}
		static constexpr auto enum_integer_by_id(std::string_view id) -> std::optional<underlying_enum_type>
		{
			constexpr auto enumIds = enum_ids();
			auto index = detail::find_index(enumIds, id);
			if (!index.has_value())
				return {};

			return (underlying_enum_type)enum_values<AllNodes>()[index.value()];
		}
		static constexpr auto enum_value(detail::Integral auto integer)
		{
			static_assert(std::is_same_v<decltype(integer), underlying_enum_type>, "Underlying type doesn't match");

			constexpr auto enumValues = enum_values<AllNodes>();
			for (auto value : enumValues)
				if ((underlying_enum_type)value == integer)
					return std::optional<typename E::Value>{ value };

			return std::optional<typename E::Value>{};
		}
		static constexpr auto enum_value(std::string_view enumString)
		{
			constexpr auto enumStrings = enum_strings<AllNodes>(false);
			auto index = detail::find_index(enumStrings, enumString);
			if (!index.has_value())
				return std::optional<typename E::Value>{};

			return std::optional{ enum_values<AllNodes>()[index.value()] };
		}
		static constexpr auto enum_value_by_id(std::string_view id)
		{
			constexpr auto enumIds = enum_ids();
			auto index = detail::find_index(enumIds, id);
			if (!index.has_value())
				return std::optional<typename E::Value>{};

			return std::optional{ enum_values<AllNodes>()[index.value()] };
		}
    private:
        // predicate is a lambda that takes the current nested_enum struct as type template parameter and
        // returns a bool whether this is the searched for type (i.e. the enum value searched for is contained inside)
		template<auto Predicate>
		static constexpr auto find_type_recursive_internal()
		{
			if constexpr (Predicate.template operator()<E>())
				return std::optional{ std::type_identity<E>{} };
            else
            {
                if constexpr (detail::get_tuple_size<decltype(E::subtypes_)>() == 0)
                    return std::optional<std::type_identity<E>>{};
                else
                {
                    constexpr auto subtypesToQuery = detail::filter<[]<typename T>(T &&) -> bool
                        {
                            return detail::is_complete_type_v<typename detail::pure_type_t<T>::type>;
                        },
                        E::subtypes_>();

                    if constexpr (detail::get_tuple_size<decltype(subtypesToQuery)>() == 0)
                        return std::optional<std::type_identity<E>>{};
                    else
                    {
                        constexpr auto subtypesResults = detail::apply_one<false>([]<typename T>(T &&)
                            {
                                using type = typename detail::pure_type_t<T>::type;
                                return type::template find_type_recursive_internal<Predicate>();
                            },
                            subtypesToQuery);
                        
                        constexpr auto areIncluded = detail::apply_one([](auto &&optional) -> bool
                            {
                                return optional.has_value();
                            }, subtypesResults);

                        constexpr auto index = [](const auto container, const auto value)
                        {
                            auto first = container.begin();
                            auto last = container.end();

                            for (; first != last; ++first)
                                if (*first == value)
                                    return std::optional{ first - container.begin() };
                        
                            return std::optional<std::ptrdiff_t>{};
                        }(areIncluded, true);

                        if constexpr (!index.has_value())
                            return std::optional<std::type_identity<E>>{};
                        else
                            return std::get<index.value()>(subtypesResults);
                    }
                }
            }
		}
    public:
		static constexpr auto enum_string_recursive(detail::Enum auto value, bool clean = false) -> std::optional<std::string_view>
		{
            constexpr auto typeId = find_type_recursive_internal<[]<typename T>()
                {
                    return std::is_same_v<decltype(value), typename T::Value>;
                }>();
            
            if constexpr (!typeId.has_value())
                return {};
            else
            {
                using type = typename detail::pure_type_t<decltype(typeId.value())>::type;
                return type::enum_string(value, clean);
            }
        }
        static constexpr auto enum_id_recursive(detail::Enum auto value) -> std::optional<std::string_view>
		{
            constexpr auto typeId = find_type_recursive_internal<[]<typename T>()
                {
                    return std::is_same_v<decltype(value), typename T::Value>;
                }>();
            
            if constexpr (!typeId.has_value())
                return {};
            else
            {
                using type = typename detail::pure_type_t<decltype(typeId.value())>::type;
                return type::enum_id(value);
            }
        }
        static constexpr auto enum_integer_recursive(detail::Enum auto value)
		{
            constexpr auto typeId = find_type_recursive_internal<[]<typename T>()
                {
                    return std::is_same_v<decltype(value), typename T::Value>;
                }>();
            
            if constexpr (!typeId.has_value())
                return std::optional<underlying_enum_type>{};
            else
            {
                using type = typename detail::pure_type_t<decltype(typeId.value())>::type;
                return type::enum_integer(value);
            }
        }
        template<fixed_string enumString>
        static constexpr auto enum_integer_recursive()
		{
            constexpr auto typeId = find_type_recursive_internal<[]<typename T>()
                {
                    constexpr auto enumInteger = T::enum_integer(std::string_view(enumString));
                    return enumInteger.has_value();
                }>();
            
            if constexpr (!typeId.has_value())
                return std::optional<underlying_enum_type>{};
            else
            {
                using type = typename detail::pure_type_t<decltype(typeId.value())>::type;
                return type::enum_integer(std::string_view(enumString));
            }
        }
        template<fixed_string id>
		static constexpr auto enum_integer_recursive_by_id()
		{
            constexpr auto typeId = find_type_recursive_internal<[]<typename T>()
                {
                    constexpr auto value = T::enum_integer_by_id(std::string_view(id));
                    return value.has_value();
                }>();
            
            if constexpr (!typeId.has_value())
                return std::optional<underlying_enum_type>{};
            else
            {
                using type = typename detail::pure_type_t<decltype(typeId.value())>::type;
                return type::enum_integer_by_id(std::string_view(id));
            }
		}
        template<fixed_string enumString>
		static constexpr auto enum_value_recursive()
		{
            constexpr auto typeId = find_type_recursive_internal<[]<typename T>()
                {
                    constexpr auto value = T::enum_value(std::string_view(enumString));
                    return value.has_value();
                }>();
            
            if constexpr (!typeId.has_value())
                return std::optional<typename E::Value>{};
            else
            {
                using type = typename detail::pure_type_t<decltype(typeId.value())>::type;
                return type::enum_value(std::string_view(enumString));
            }
		}
        template<fixed_string id>
		static constexpr auto enum_value_recursive_by_id()
		{
            constexpr auto typeId = find_type_recursive_internal<[]<typename T>()
                {
                    constexpr auto value = T::enum_value_by_id(std::string_view(id));
                    return value.has_value();
                }>();
            
            if constexpr (!typeId.has_value())
                return std::optional<typename E::Value>{};
            else
            {
                using type = typename detail::pure_type_t<decltype(typeId.value())>::type;
                return type::enum_value_by_id(std::string_view(id));
            }
		}

        // the following functions might stop working at some point
        // because they rely on the fact that every enum has the same underlying type
        // might delete at some point
        static constexpr auto enum_integer_recursive(std::string_view enumString)
		{
			constexpr auto enumStrings = enum_strings<AllNodes>(false);
			auto index = detail::find_index(enumStrings, enumString);
			if (index.has_value())
				return std::optional{ (underlying_enum_type)enum_values<AllNodes>()[index.value()] };

			if constexpr (detail::get_tuple_size<decltype(E::subtypes_)>() == 0)
				return std::optional<underlying_enum_type>{};
			else
			{
				constexpr auto subtypesToQuery = detail::filter<[]<typename T>(T &&) -> bool
					{
						return detail::is_complete_type_v<typename detail::pure_type_t<T>::type>;
					},
					E::subtypes_>();

				if constexpr (detail::get_tuple_size<decltype(subtypesToQuery)>() == 0)
					return std::optional<underlying_enum_type>{};
				else
				{
					auto subtypesResults = detail::apply_one([&]<typename T>(T &&)
						{
							using type = typename detail::pure_type_t<T>::type;
							return type::enum_integer_recursive(enumString);
						},
						subtypesToQuery);

					for (auto &optional : subtypesResults)
						if (optional.has_value())
							return optional;

					return std::optional<underlying_enum_type>{};
				}
			}
		}
		static constexpr auto enum_integer_recursive_by_id(std::string_view id)
		{
			constexpr auto enumIds = enum_ids();
			auto index = detail::find_index(enumIds, id);
			if (index.has_value())
				return std::optional{ (underlying_enum_type)enum_values<AllNodes>()[index.value()] };

			if constexpr (detail::get_tuple_size<decltype(E::subtypes_)>() == 0)
				return std::optional<underlying_enum_type>{};
			else
			{
				constexpr auto subtypesToQuery = detail::filter<[]<typename T>(T &&) -> bool
					{
						return detail::is_complete_type_v<typename detail::pure_type_t<T>::type>;
					},
					E::subtypes_>();

				if constexpr (detail::get_tuple_size<decltype(subtypesToQuery)>() == 0)
					return std::optional<underlying_enum_type>{};
				else
				{
					auto subtypesResults = detail::apply_one([&]<typename T>(T &&)
						{
							using type = typename detail::pure_type_t<T>::type;
							return type::enum_integer_recursive_by_id(id);
						},
						subtypesToQuery);

					for (auto &optional : subtypesResults)
						if (optional.has_value())
							return optional;

					return std::optional<underlying_enum_type>{};
				}
			}
		}
	};

    template <typename T>
	constexpr bool operator==(const nested_enum<T> &left, const nested_enum<T> &right) noexcept
	{ return static_cast<const T &>(left).value == static_cast<const T &>(right).value; }

    template <typename T>
	constexpr bool operator==(const nested_enum<T> &left, const typename T::Value &right) noexcept
	{ return static_cast<const T &>(left).value == right; }

    template <typename T>
	constexpr bool operator==(const typename T::Value &left, const nested_enum<T> &right) noexcept
	{ return left == static_cast<const T &>(right).value; }

    template<class E>
    concept NestedEnum = detail::DerivedOrIs<E, nested_enum<E>>;

	#define NESTED_ENUM_EXPAND(...) NESTED_ENUM_EXPAND4(NESTED_ENUM_EXPAND4(NESTED_ENUM_EXPAND4(NESTED_ENUM_EXPAND4(__VA_ARGS__))))
	#define NESTED_ENUM_EXPAND4(...) NESTED_ENUM_EXPAND3(NESTED_ENUM_EXPAND3(NESTED_ENUM_EXPAND3(NESTED_ENUM_EXPAND3(__VA_ARGS__))))
	#define NESTED_ENUM_EXPAND3(...) NESTED_ENUM_EXPAND2(NESTED_ENUM_EXPAND2(NESTED_ENUM_EXPAND2(NESTED_ENUM_EXPAND2(__VA_ARGS__))))
	#define NESTED_ENUM_EXPAND2(...) NESTED_ENUM_EXPAND1(NESTED_ENUM_EXPAND1(NESTED_ENUM_EXPAND1(NESTED_ENUM_EXPAND1(__VA_ARGS__))))
	#define NESTED_ENUM_EXPAND1(...) __VA_ARGS__

	#define NESTED_ENUM_ID_WITH_COMMA(x, ...) x __VA_OPT__(,)
	#define NESTED_ENUM_STRING_VIEW_WITH_COMMA(x, ...) std::string_view(x) __VA_OPT__(,)
	#define NESTED_ENUM_PARENS ()

	#define NESTED_ENUM_FOR_EACH(helper, macro, ...)                                \
		__VA_OPT__(NESTED_ENUM_EXPAND(helper(macro, __VA_ARGS__)))

	#define NESTED_ENUM_GET_FIRST_OF_ONE(macro, a1, ...)                            \
		macro(a1, __VA_ARGS__)                                                        \
		__VA_OPT__(NESTED_ENUM_GET_FIRST_OF_ONE_AGAIN NESTED_ENUM_PARENS (macro, __VA_ARGS__))
	#define NESTED_ENUM_GET_FIRST_OF_ONE_AGAIN() NESTED_ENUM_GET_FIRST_OF_ONE

	#define NESTED_ENUM_GET_FIRST_OF_TWO(macro, a1, a2, ...)                        \
		macro(a1, __VA_ARGS__)                                                        \
		__VA_OPT__(NESTED_ENUM_GET_FIRST_OF_TWO_AGAIN NESTED_ENUM_PARENS (macro, __VA_ARGS__))
	#define NESTED_ENUM_GET_FIRST_OF_TWO_AGAIN() NESTED_ENUM_GET_FIRST_OF_TWO

	#define NESTED_ENUM_GET_SECOND_OF_TWO(macro, a1, a2, ...)                       \
		macro(a2, __VA_ARGS__)                                                        \
		__VA_OPT__(NESTED_ENUM_GET_SECOND_OF_TWO_AGAIN NESTED_ENUM_PARENS (macro, __VA_ARGS__))
	#define NESTED_ENUM_GET_SECOND_OF_TWO_AGAIN() NESTED_ENUM_GET_SECOND_OF_TWO

	#define NESTED_ENUM_DECLARE_STRUCT(typeName, ...) struct typeName;
	#define NESTED_ENUM_STRUCT_IDENTITY(typeName, ...) std::type_identity<struct typeName> __VA_OPT__(,)


	#define NESTED_ENUM(typeName, ...)                                                                                          \
	struct typeName : ::nested_enum::nested_enum<struct typeName>                                                               \
	{                                                                                                                           \
        friend struct nested_enum<struct typeName>;                                                                             \
        using nested_enum::nested_enum;                                                                                         \
        using nested_enum::operator=;                                                                                           \
                                                                                                                                \
        enum Value : underlying_enum_type { __VA_ARGS__ };                                                                      \
        Value value{};                                                                                                          \
                                                                                                                                \
        constexpr operator Value() const noexcept { return value; }                                                             \
                                                                                                                                \
        NESTED_ENUM_FOR_EACH(NESTED_ENUM_GET_FIRST_OF_ONE, NESTED_ENUM_DECLARE_STRUCT, __VA_ARGS__)                             \
                                                                                                                                \
        static constexpr std::array<std::string_view, 0> ids{};																	\
                                                                                                                                \
        static constexpr std::tuple<NESTED_ENUM_FOR_EACH(NESTED_ENUM_GET_FIRST_OF_ONE,                                          \
                NESTED_ENUM_STRUCT_IDENTITY, __VA_ARGS__)> subtypes_{};                                                         \
                                                                                                                                \
        static constexpr std::array enumValues =                                                                                \
                ::nested_enum::detail::get_array_of_values<Value, #__VA_ARGS__, false>();                                       \
	};



	#define NESTED_ENUM_WITH_IDS(typeName, ...)                                                                                 \
	struct typeName : ::nested_enum::nested_enum<struct typeName>                                                               \
	{                                                                                                                           \
        friend struct nested_enum<struct typeName>;                                                                             \
        using nested_enum::nested_enum;                                                                                         \
        using nested_enum::operator=;                                                                                           \
                                                                                                                                \
        enum Value : underlying_enum_type                                                                                       \
                { NESTED_ENUM_FOR_EACH(NESTED_ENUM_GET_FIRST_OF_TWO, NESTED_ENUM_ID_WITH_COMMA, __VA_ARGS__) };                 \
        Value value{};                                                                                                          \
                                                                                                                                \
        constexpr operator Value() const noexcept { return value; }                                                             \
                                                                                                                                \
        NESTED_ENUM_FOR_EACH(NESTED_ENUM_GET_FIRST_OF_TWO, NESTED_ENUM_DECLARE_STRUCT, __VA_ARGS__)                             \
                                                                                                                                \
        static constexpr auto ids = std::to_array<std::string_view>																\
                ({ NESTED_ENUM_FOR_EACH(NESTED_ENUM_GET_SECOND_OF_TWO, NESTED_ENUM_ID_WITH_COMMA, __VA_ARGS__) });              \
                                                                                                                                \
        static constexpr std::tuple<NESTED_ENUM_FOR_EACH(NESTED_ENUM_GET_FIRST_OF_TWO,                                          \
                NESTED_ENUM_STRUCT_IDENTITY, __VA_ARGS__)> subtypes_{};                                                         \
                                                                                                                                \
        static constexpr std::array enumValues =                                                                                \
                ::nested_enum::detail::get_array_of_values<Value, #__VA_ARGS__, true>();                                        \
	};

	#define NESTED_ENUM_STRING(enum_name, enum_value, clean) enum_name::enum_string(enum_name::enum_value, clean)
	#define NESTED_ENUM_STRING_AND_ID(enum_name, enum_value) enum_name::enum_string_and_id(enum_name::enum_value)
	#define NESTED_ENUM_INTEGER(enum_name, enum_value) enum_name::enum_integer(enum_name::enum_value)
}
