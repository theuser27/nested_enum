#ifndef NESTED_ENUM_HPP
#define NESTED_ENUM_HPP

#define NESTED_ENUM_VERSION_MAJOR 0
#define NESTED_ENUM_VERSION_MINOR 3
#define NESTED_ENUM_VERSION_PATCH 3

#include <cstdint>
#include <type_traits>
#include <utility>
#include <array>
#include <string_view>
#include <optional>
#include <tuple>

#ifndef NESTED_ENUM_DEFAULT_ENUM_TYPE
  #define NESTED_ENUM_DEFAULT_ENUM_TYPE ::std::int32_t
#endif

#ifdef NESTED_ENUM_SUPPRESS_MULTIPLE_RESULTS_ASSERT
  #define NESTED_ENUM_SUPPRESS_MULTIPLE_RESULTS_ASSERT 0
#endif

namespace nested_enum
{
  template <std::size_t N>
  struct fixed_string
  {
    constexpr fixed_string() = default;
    constexpr fixed_string(const char(&array)[N + 1]) noexcept
    {
      for (std::size_t i = 0; i < N; ++i)
        data[i] = array[i];

      data[N] = '\0';
    }

    template<std::size_t M>
    [[nodiscard]] constexpr auto append(fixed_string<M> other) const noexcept
    {
      fixed_string<N + M> result;
      result.data[N + M] = '\0';

      for (std::size_t i = 0; i < N; ++i)
        result.data[i] = data[i];

      for (std::size_t i = 0; i < M; ++i)
        result.data[N + i] = other.data[i];

      return result;
    }

    // concatenates with null terminator
    template<std::size_t M>
    [[nodiscard]] constexpr auto append_full(fixed_string<M> other) const noexcept
    {
      fixed_string<N + 1 + M> result;

      for (std::size_t i = 0; i < N; ++i)
        result.data[i] = data[i];
      result.data[N] = '\0';

      for (std::size_t i = 0; i < M; ++i)
        result.data[N + 1 + i] = other.data[i];
      result.data[N + 1 + M] = '\0';

      return result;
    }

    [[nodiscard]] constexpr auto operator<=>(const fixed_string &) const = default;
    [[nodiscard]] constexpr operator std::string_view() const noexcept { return { data.data(), N }; }
    [[nodiscard]] static constexpr std::size_t size() noexcept { return N; }

    std::array<char, N + 1> data;
  };

  template <std::size_t N>
  fixed_string(const char(&)[N])->fixed_string<N - 1>;

  namespace detail
  {
    template<typename T>
    concept Enum = std::is_enum_v<T>;

    template <class T>
    inline constexpr auto is_complete_type_v = requires{ sizeof(T); };

    template <typename ... Ts>
    struct type_list
    {
      static constexpr auto size = sizeof...(Ts);

      template <typename ... Us>
      constexpr auto operator+(type_list<Us...>) const noexcept
      {
        return type_list<Ts..., Us...>{};
      }

      template <typename ... Us>
      constexpr bool operator==(type_list<Us...>) const noexcept
      {
        return std::is_same_v<type_list<Ts...>, type_list<Us...>>;
      }
    };

    template<typename T, typename ... Ts>
    T get_first_type(type_list<T, Ts...>) { return T{}; }

    template<typename T = int>
    struct opt { bool isInitialised = false; T value = 0; };

    inline constexpr fixed_string scopeResolution = "::";

    template<typename T>
    constexpr auto find_index(const auto &container, const T &value)
    {
      auto first = container.begin();
      auto last = container.end();

      if constexpr (std::is_same_v<typename std::remove_cvref_t<decltype(container)>::value_type, std::optional<std::remove_cvref_t<T>>>)
      {
        for (; first != last; ++first)
          if (first->has_value() && first->value() == value)
            return std::optional{ (std::size_t)(first - container.begin()) };        
      }
      else
      {
        static_assert(std::is_same_v<typename std::remove_cvref_t<decltype(container)>::value_type, std::remove_cvref_t<T>>);

        for (; first != last; ++first)
          if (*first == value)
            return std::optional{ (std::size_t)(first - container.begin()) };
      }

      return std::optional<std::size_t>{};
    }

    template<typename E, typename T, opt<T> ... values>
    consteval auto get_array_of_values()
    {
      std::array<E, sizeof...(values)> enumValues{};
      std::size_t index = 0;
      T previous = 0;

      for (auto current : std::initializer_list<opt<T>>{ values... })
      {
        if (current.isInitialised)
          previous = current.value;
        
        enumValues[index++] = (E)(previous++);
      }

      return enumValues;
    }

    template<fixed_string type, fixed_string value, fixed_string ... values>
    consteval auto get_string_values()
    {
      static_assert(type.size() > 0);
      constexpr auto current = type.append(scopeResolution.append(value));

      if constexpr (sizeof...(values) == 0)
        return current.append_full(fixed_string{ "" });
      else
        return current.append_full(get_string_values<type, values...>());
    }

    // bless their soul https://stackoverflow.com/a/54932626
    namespace tuple_magic
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

      constexpr decltype(auto) flatten(auto t)
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

    template<typename A>
    inline constexpr auto is_std_array_v = requires
    {
      requires std::is_same_v<A, std::array<typename A::value_type, std::tuple_size_v<A>>>;
    };

    template<class TupleOrArray>
    constexpr auto tuple_of_arrays_to_array(TupleOrArray &&tupleOrArray)
    {
      if constexpr (is_std_array_v<TupleOrArray>)
        return tupleOrArray;
      else
      {
        auto function = []<typename T, std::size_t M, typename ... Args>(std::array<T, M> first, Args ... args)
        {
          if constexpr (sizeof...(Args) == 0)
            return first;
          else
          {
            constexpr std::size_t N = M + (args.size() + ...);

            std::array<T, N> newArray;
            std::size_t index = 0;

            auto iterate = [&index, &newArray](auto current)
            {
              for (std::size_t i = 0; i < current.size(); i++)
                newArray[index + i] = current[i];

              index += current.size();
            };

            iterate(first);
            (iterate(args), ...);

            return newArray;
          }
        };

        return [&]<std::size_t ... Indices>(std::index_sequence<Indices...>)
        {
          return function(std::get<Indices>(tupleOrArray)...);
        }(std::make_index_sequence<std::tuple_size_v<TupleOrArray>>{});
      }
    }

    constexpr std::string_view get_substring(std::string_view allStrings, std::size_t index, bool clean) noexcept
    {
      std::size_t end = 0;
      std::size_t count = 0;
      while (count < index)
        if (allStrings[end++] == '\0')
          ++count;

      std::string_view view = std::string_view(&allStrings[end]);

      if (clean)
      {
        std::string_view scope = "::";
        view.remove_prefix(view.rfind(scope) + scope.length());
      }

      return view;
    }

    template<fixed_string prefix = "">
    constexpr auto get_prefix() noexcept { return prefix; }

    template<typename T, typename E>
    concept this_underlying_type = std::is_same_v<T, typename E::underlying_type>;
  }

  #define TEST_INCLUSIVENESS(selection, type) (selection == All || \
    (!detail::is_complete_type_v<type> && selection == Outer) ||   \
    (detail::is_complete_type_v<type> && ((type::isLeaf && selection == Outer) || (!type::isLeaf && selection == Inner))))

  // Inner - enum values that are themselves enums
  // Outer - enum values that are NOT enums
  // All   - both inner and outer enum values 
  enum InnerOuterAll { Inner, Outer, All };

  template<class E, class P = E>
  struct nested_enum
  {
    using type = E;
    using parent = P;

    template<class, class>
    friend struct nested_enum;
    using nested_enum_tag = void;

    // returns the reflected type name of the enum 
    static constexpr auto name(bool clean = false) noexcept -> std::string_view
    {
      std::string_view name = E::internalName;
      if (clean)
      {
        std::string_view scope = "::";
        name.remove_prefix(name.rfind(scope) + scope.length());
      }
      return name;
    }
    // returns the id of the type name (if it has one)
    static constexpr auto id() noexcept -> std::optional<std::string_view>
    {
      if constexpr (std::is_same_v<E, P>)
        return {};
      else
        return P::enum_id(E::value());
    }
    // returns the integer of the type name (if it has one)
    static constexpr auto integer() noexcept
    {
      return static_cast<typename E::underlying_type>(E::value());
    }
    // returns the global id of the topmost parent
    static constexpr auto global_prefix() -> std::string_view
    {
      if constexpr (std::is_same_v<E, P>)
        return std::string_view{ E::internalGlobalPrefix };
      else
        return parent::global_prefix();
    }
    // returns an instance of this type with the given value if it exist
    template<typename T> requires std::is_integral_v<T> || std::is_floating_point_v<T>
    static constexpr auto make_enum(T t) -> std::optional<E>
    {
      return enum_value(static_cast<typename E::underlying_type>(t));
    }


    // returns the string of the currently held value
    constexpr auto enum_name(bool clean = false) const noexcept -> std::string_view
    {
      return enum_name(static_cast<const E &>(*this), clean).value();
    }
    // returns the id of the currently held value
    constexpr auto enum_id() const noexcept -> std::optional<std::string_view>
    {
      return enum_id(static_cast<const E &>(*this));
    }
    // returns the string and id of the currently held value
    constexpr auto enum_name_and_id(bool clean = false) const noexcept -> std::pair<std::string_view, std::optional<std::string_view>>
    {
      return enum_name_and_id(static_cast<const E &>(*this), clean).value();
    }
    // returns the integer representation of the currently held value
    constexpr auto enum_integer() const noexcept
    {
      return (typename E::underlying_type)static_cast<const E &>(*this).internalValue;
    }

  protected:
    template<std::size_t N>
    static constexpr auto create_name(fixed_string<N> name)
    {
      if constexpr (std::is_same_v<E, P>)
      {
        if constexpr (E::internalGlobalPrefix.size() == 0)
          return name;
        else
        {
          auto fullName = E::internalGlobalPrefix.append(detail::scopeResolution.append(name));
          return fullName;
        }
      }
      else
      {
        auto fullName = P::internalName.append(detail::scopeResolution.append(name));
        return fullName;
      }
    }
    // returns a tuple of the enum subtypes that satisfy the selection
    template<InnerOuterAll Selection, typename ... Ts>
    static constexpr auto get_needed_values(detail::type_list<Ts...>) noexcept
    {
      std::size_t size = 0;
      std::size_t index = 0;
      std::array<bool, E::enumValues.size()> valuesNeeded{};
      auto getValues = [&]<typename T>()
      {
        if constexpr (TEST_INCLUSIVENESS(Selection, T))
        {
          size++;
          valuesNeeded[index] = true;
        }
        else
          valuesNeeded[index] = false;
        index++;
      };

      (getValues.template operator()<Ts>(), ...);

      return std::pair{ valuesNeeded, size };
    }
  public:
    // returns the values inside this enum that satisfy the Selection
    template<InnerOuterAll Selection = All>
    static constexpr auto enum_values() noexcept
    {
      if constexpr (E::enumValues.size() == 0 || Selection == All)
      {
        constexpr auto enumValues = []()
        {
          auto values = E::template internal_get_array_of_type<E::enumValues.size()>();
          for (std::size_t i = 0; i < values.size(); ++i)
            values[i] = E{ E::enumValues[i] };
          return values;
        }();
        return enumValues;
      }
      else
      {
        constexpr auto result = []()
        {
          constexpr auto valuesNeeded = get_needed_values<Selection>(E::subtypes);

          auto values = E::template internal_get_array_of_type<valuesNeeded.second>();
          for (std::size_t i = 0, j = 0; i < valuesNeeded.first.size(); i++)
            if (valuesNeeded.first[i])
              values[j++] = E{ E::enumValues[i] };
          return values;
        }();

        return result;
      }
    }
    // returns the values count inside this enum that satisfy the selection
    static constexpr auto enum_count(InnerOuterAll selection = All) noexcept -> std::size_t
    {
      if constexpr (E::enumValues.size() == 0)
        return 0;
      else
      {
        if (selection == All)
          return E::enumValues.size();

        return [&]<typename ... Ts>(detail::type_list<Ts...>)
        {
          std::size_t total = 0;
          auto getTotal = [&]<typename T>()
          {
            if (TEST_INCLUSIVENESS(selection, T))
              total++;
          };

          (getTotal.template operator()<Ts>(), ...);
          return total;
        }(E::subtypes);
      }
    }
    // returns the underlying integers of enum values that satisfy the selection
    template<InnerOuterAll Selection = All>
    static constexpr auto enum_integers() noexcept
    {
      constexpr auto integerArray = []()
      {
        auto enumValues = enum_values<Selection>();
        std::array<typename E::underlying_type, enumValues.size()> result;
        for (std::size_t i = 0; i < result.size(); i++)
          result[i] = (typename E::underlying_type)enumValues[i].internalValue;

        return result;
      }();

      return integerArray;
    }
    // returns the ids of enum values that satisfy the selection
    template<InnerOuterAll Selection = All>
    static constexpr auto enum_ids() noexcept
    {
      if constexpr (E::enumIds.size() == 0 || Selection == All)
        return E::enumIds;
      else
      {
        constexpr auto result = []()
        {
          constexpr auto valuesNeeded = get_needed_values<Selection>(E::subtypes);

          std::array<std::optional<std::string_view>, valuesNeeded.second> values;
          for (std::size_t i = 0, j = 0; i < valuesNeeded.first.size(); i++)
            if (valuesNeeded.first[i])
              values[j++] = E::enumIds[i];
          
          return values;
        }();

        return result;
      }
    }
    // returns the reflected strings of enum values that satisfy the selection
    // "clean" refers to whether only the enum value name or its entire path is returned
    template<InnerOuterAll Selection = All>
    static constexpr auto enum_names(bool clean = false) noexcept
    {
      if constexpr (enum_count(Selection) == 0)
      {
        // if this isn't assigned to a constexpr variable msvc won't recognise the function as constexpr
        constexpr auto enumNames = std::array<std::string_view, 0>{};
        return enumNames;
      }
      else
      {
        constexpr auto generateStrings = []<bool cleanString>()
        {
          constexpr auto valuesNeeded = get_needed_values<Selection>(E::subtypes);

          std::array<std::string_view, valuesNeeded.second> values;
          for (std::size_t i = 0, j = 0; i < valuesNeeded.first.size(); i++)
            if (valuesNeeded.first[i])
              values[j++] = detail::get_substring(std::string_view(E::enumNames), i, cleanString);
          
          return values;
        };

        constexpr auto enumNames = generateStrings.template operator()<false>();
        constexpr auto enumNamesClean = generateStrings.template operator()<true>();
        if (!clean)
          return enumNames;
        else
          return enumNamesClean;
      }
    }
    // returns the reflected strings and ids of enum values that satisfy the selection
    // "clean" refers to whether only the enum value name or its entire path is returned
    template<InnerOuterAll Selection = All>
    static constexpr auto enum_names_and_ids(bool clean = false) noexcept
    {
      if constexpr (enum_count(Selection) == 0)
        return std::array<std::pair<std::string_view, std::optional<std::string_view>>, 0>{};
      else
      {
        constexpr auto stringsAndIds = []()
        {
          constexpr auto valuesNeeded = get_needed_values<Selection>(E::subtypes);

          std::array<std::pair<std::string_view, std::optional<std::string_view>>, valuesNeeded.second> values{};
          for (std::size_t i = 0, j = 0; i < valuesNeeded.first.size(); i++)
            if (valuesNeeded.first[i])
              values[j++] = { detail::get_substring(E::enumNames, i, false), E::enumIds[i] };

          return values;
        }();

        constexpr auto stringsAndIdsClean = [&]()
        {
          std::array<std::pair<std::string_view, std::optional<std::string_view>>, stringsAndIds.size()> values;
          std::string_view scope = "::";

          for (std::size_t i = 0; i < stringsAndIds.size(); ++i)
          {
            auto string = stringsAndIds[i].first;
            string.remove_prefix(string.rfind(scope) + scope.length());
            values[i] = std::pair{ string, stringsAndIds[i].second };
          }

          return values;
        }();

        if (!clean)
          return stringsAndIds;
        else
          return stringsAndIdsClean;
      }
    }
    // returns a tuple of the enum subtypes that satisfy the selection
    template<InnerOuterAll Selection = All>
    static constexpr auto enum_subtypes() noexcept
    {
      if constexpr (E::enumValues.size() == 0)
        return std::tuple<>{};
      else
      {
        constexpr auto list = []<typename ... Ts>(detail::type_list<Ts...>)
        {
          auto recurse = []<typename T>()
          {
            if constexpr (TEST_INCLUSIVENESS(Selection, T))
              return detail::type_list<T>{};
            else
              return detail::type_list<>{};
          };

          auto list = (recurse.template operator()<Ts>() + ...);
          return list;
        }(E::subtypes);

        return []<typename ... Ts>(detail::type_list<Ts...>)
        {
          return std::tuple<std::type_identity<Ts>...>{};
        }(list);
      }
    }

  private:
    template<typename ... Ts>
    static constexpr std::size_t enum_count_recursive_internal(InnerOuterAll selection, detail::type_list<Ts...>) noexcept
    {
      std::size_t count = (selection == Inner) || (selection == All);
      if constexpr (sizeof...(Ts) == 0)
        return count;
      else
      {
        auto subtypesSizes = [&]<typename T>() -> std::size_t
        {
          if constexpr (detail::is_complete_type_v<T> && !T::isLeaf)
            return T::enum_count_recursive_internal(selection, T::subtypes);
          else
            return (selection == Outer) || (selection == All);
        };

        return count + (subtypesSizes.template operator()<Ts>() + ...);
      }
    }
  public:
    // returns are recursive count of all enum values in the subtree that satisfy the selection
    static constexpr std::size_t enum_count_recursive(InnerOuterAll selection = All) noexcept
    {
      std::size_t count = enum_count_recursive_internal(selection, E::subtypes);
      if ((selection == Inner) || (selection == All))
        return --count;
      return count;
    }
  private:
    template<auto Predicate, InnerOuterAll Selection>
    static constexpr auto return_recursive_internal() noexcept
    {
      if constexpr (E::enumValues.size() == 0)
        return std::tuple<>{};
      else
      {
        constexpr auto values = Predicate.template operator()<E>();

        // getting an array of values we need
        constexpr auto subtypesToQuery = [&]<typename ... Ts>(detail::type_list<Ts...>)
        {
          // new tuple of only the inner enum nodes
          auto checkQueries = []<typename T>()
          {
            // does the enum exist
            if constexpr (!detail::is_complete_type_v<T>)
              return detail::type_list<>{};
            else
            {
              // does the enum have any values
              if constexpr (T::subtypes.size == 0)
                return detail::type_list<>{};
              // different branches based on what we want
              else if constexpr ((Selection == All) || (Selection == Outer))
                return detail::type_list<T>{};
              else
              {
                auto checkInner = []<typename ... Us>(detail::type_list<Us...>)
                {
                  auto checkIndividual = []<typename U>() -> bool
                  {
                    if constexpr (detail::is_complete_type_v<U>)
                      return !U::isLeaf;
                    else
                      return false;
                  };
                  return false || (checkIndividual.template operator()<Us>() || ...);
                };

                if constexpr (checkInner(T::subtypes))
                  return detail::type_list<T>{};
                else
                  return detail::type_list<>{};
              }
            }
          };

          return (checkQueries.template operator()<Ts>() + ...);
        }(E::subtypes);

        // returning only the values if we don't have any subtypes to query
        if constexpr (subtypesToQuery.size == 0)
          return std::make_tuple(values);
        else
        {
          constexpr auto subtypesTuples = []<typename ... Ts>(detail::type_list<Ts...>)
          {
            auto expand = []<typename T>() { return T::template return_recursive_internal<Predicate, Selection>(); };
            return std::make_tuple(expand.template operator()<Ts>()...);
          }(subtypesToQuery);

          if constexpr (values.size() == 0)
            return subtypesTuples;
          else
            return std::tuple_cat(std::make_tuple(values), subtypesTuples);
        }
      }
    }
  public:
    // returns std::tuple<std::array<Enum, ?>...> 
    // of all enum values in the subtree that satisfy the selection
    template<InnerOuterAll Selection = All>
    static constexpr auto enum_values_recursive() noexcept
    {
      constexpr auto predicate = []<typename T>()
      {
        return T::template enum_values<Selection>();
      };

      constexpr auto tuple = detail::tuple_magic::flatten(return_recursive_internal<predicate, Selection>());
      return tuple;
    }
    // returns std::tuple<std::array<std::string_view, ?>...> 
    // of the reflected names of all enum values in the subtree that satisfy the selection
    // if flattenTuple == true, the tuple of arrays will be flattened to a single array with all strings inside
    template<InnerOuterAll Selection = All, bool flattenTuple = true, bool clean = false>
    static constexpr auto enum_names_recursive() noexcept
    {
      constexpr auto predicate = []<typename T>()
      {
        return T::template enum_names<Selection>(clean);
      };

      if constexpr (flattenTuple)
      {
        constexpr auto flattenedTuple = detail::tuple_of_arrays_to_array(detail::tuple_magic::flatten(return_recursive_internal<predicate, Selection>()));
        return flattenedTuple;
      }
      else
      {
        constexpr auto tuple = detail::tuple_magic::flatten(return_recursive_internal<predicate, Selection>());
        return tuple;
      }
    }
    // returns std::tuple<std::array<std::optional<std::string_view>, ?>...> 
    // of the ids of all enum values in the subtree that satisfy the selection
    // if flattenTuple == true, the tuple of arrays will be flattened to a single array with all strings inside
    template<InnerOuterAll Selection = All, bool flattenTuple = true>
    static constexpr auto enum_ids_recursive() noexcept
    {
      constexpr auto predicate = []<typename T>()
      {
        return T::template enum_ids<Selection>();
      };

      if constexpr (flattenTuple)
      {
        constexpr auto flattenedTuple = detail::tuple_of_arrays_to_array(detail::tuple_magic::flatten(return_recursive_internal<predicate, Selection>()));
        return flattenedTuple;
      }
      else
      {
        constexpr auto tuple = detail::tuple_magic::flatten(return_recursive_internal<predicate, Selection>());
        return tuple;
      }
    }
    // returns std::tuple<std::array<std::pair<std::string_view, std::optional<std::string_view>>, ?>...>
    // of the ids and reflected strings of all enum values in the subtree that satisfy the selection
    // if flattenTuple == true, the tuple of arrays will be flattened to a single array with all pairs of strings inside
    template<InnerOuterAll Selection = All, bool flattenTuple = true, bool clean = false>
    static constexpr auto enum_names_and_ids_recursive() noexcept
    {
      constexpr auto predicate = []<typename T>()
      {
        return T::template enum_names_and_ids<Selection>(clean);
      };

      if constexpr (flattenTuple)
      {
        constexpr auto flattenedTuple = detail::tuple_of_arrays_to_array(detail::tuple_magic::flatten(return_recursive_internal<predicate, Selection>()));
        return flattenedTuple;
      }
      else
      {
        constexpr auto tuple = detail::tuple_magic::flatten(return_recursive_internal<predicate, Selection>());
        return tuple;
      }
    }

    // returns the reflected string of an enum value of this type
    static constexpr auto enum_name(E value, bool clean = false) -> std::optional<std::string_view>
    {
      auto index = detail::find_index(E::enumValues, value.internalValue);
      if (!index.has_value())
        return {};

      return enum_names<All>(clean)[index.value()];
    }
    // returns the reflected string of an enum value of this type, specified by its id
    static constexpr auto enum_name_by_id(std::string_view id, bool clean = false) -> std::optional<std::string_view>
    {
      auto index = detail::find_index(E::enumIds, id);
      if (!index.has_value())
        return {};

      return enum_names(clean)[index.value()];
    }
    // returns the id of an enum value of this type
    static constexpr auto enum_id(E value) -> std::optional<std::string_view>
    {
      auto index = detail::find_index(E::enumValues, value.internalValue);
      if (!index.has_value())
        return {};

      return enum_ids<All>()[index.value()];
    }
    // returns the id of an enum value of this type, specified by its reflected string
    static constexpr auto enum_id(std::string_view enumName) -> std::optional<std::string_view>
    {
      constexpr auto strings = enum_names<All>();
      auto index = detail::find_index(strings, enumName);
      if (!index.has_value())
        return {};

      return enum_ids<All>()[index.value()];
    }
    // returns the reflected string and id of an enum value of this type
    static constexpr auto enum_name_and_id(E value, bool clean = false) -> std::optional<std::pair<std::string_view, std::optional<std::string_view>>>
    {
      auto index = detail::find_index(E::enumValues, value.internalValue);
      if (!index.has_value())
        return {};

      return enum_names_and_ids<All>(clean)[index.value()];
    }
    // returns the underlying integer of an enum value of this type
    static constexpr auto enum_integer(E value)
    {
      return std::optional{ (typename E::underlying_type)value.internalValue };
    }
    // returns the underlying integer of an enum value of this type, specified by its reflected string
    static constexpr auto enum_integer(std::string_view enumName)
    {
      constexpr auto enumNames = enum_names<All>(false);
      auto index = detail::find_index(enumNames, enumName);
      if (!index.has_value())
        return std::optional<typename E::underlying_type>{};

      return std::optional{ (typename E::underlying_type)E::enumValues[index.value()] };
    }
    // returns the underlying integer of an enum value of this type, specified by its id
    static constexpr auto enum_integer_by_id(std::string_view id)
    {
      auto index = detail::find_index(E::enumIds, id);
      if (!index.has_value())
        return std::optional<typename E::underlying_type>{};

      return std::optional{ (typename E::underlying_type)E::enumValues[index.value()] };
    }
    // returns the enum value of this type, specified by an integer
    template<typename T> requires detail::this_underlying_type<T, E>
    static constexpr auto enum_value(T integer)
    {
      static_assert(std::is_same_v<T, typename E::underlying_type>);

      for (auto value : E::enumValues)
        if (value == (typename E::underlying_type)integer)
          return std::optional<E>{ value };

      return std::optional<E>{};
    }
    // returns the enum value of this type, specified by its reflected string
    static constexpr auto enum_value(std::string_view enumName)
    {
      constexpr auto enumNames = enum_names<All>(false);
      auto index = detail::find_index(enumNames, enumName);
      if (!index.has_value())
        return std::optional<E>{};

      return std::optional<E>{ E::enumValues[index.value()] };
    }
    // returns the enum value of this type, specified by its id
    static constexpr auto enum_value_by_id(std::string_view id)
    {
      auto index = detail::find_index(E::enumIds, id);
      if (!index.has_value())
        return std::optional<E>{};

      return std::optional<E>{ E::enumValues[index.value()] };
    }
  private:
    // predicate is a lambda that takes the current nested_enum struct as type template parameter and
    // returns a bool whether this is the searched for type (i.e. the enum value searched for is contained inside)
    template<auto Predicate, typename ... Ts>
    static constexpr auto find_type_recursive_internal(detail::type_list<Ts...>)
    {
      if constexpr (Predicate.template operator()<E>())
        return detail::type_list<E>{};
      else
      {
        if constexpr (sizeof...(Ts) == 0)
          return detail::type_list<>{};
        else
        {
          constexpr auto getResults = []<typename T>()
          {
            if constexpr (detail::is_complete_type_v<T>)
              return T::template find_type_recursive_internal<Predicate>(T::subtypes);
            else
              return detail::type_list<>{};
          };
          
          // for some reason msvc expands this branch even if sizeof...(Ts) IS 0
          // so we need to guard this expansion behind another function
          // just msvc things i guess *sigh*
        #ifdef _MSC_VER
          constexpr auto subtypesResult = [&]<typename ... Us>()
          {
            if constexpr (sizeof...(Us) == 0)
              return detail::type_list<>{};
            else
              return (getResults.template operator()<Us>() + ...);
          }.template operator()<Ts...>();
        #else
          constexpr auto subtypesResult = (getResults.template operator()<Ts>() + ...);
        #endif

          if constexpr (subtypesResult.size > 0)
          {
            #if NESTED_ENUM_SUPPRESS_MULTIPLE_RESULTS_ASSERT == 0
              static_assert(subtypesResult.size == 1, "Multiple results found for query, if this is expected define NESTED_ENUM_SUPPRESS_MULTIPLE_RESULTS_ASSERT to 1 before including the header");
            #endif

            return subtypesResult;
          }
          else
            return detail::type_list<>{};
        }
      }
    }
  public:
    // returns the reflected string of an enum value that is located somewhere in the subtree
    static constexpr auto enum_name_recursive(detail::Enum auto value, bool clean = false) -> std::optional<std::string_view>
    {
      auto typeId = find_type_recursive_internal<[]<typename T>()
      {
        return std::is_same_v<decltype(value), typename T::Value>;
      }>(E::subtypes);

      if constexpr (typeId.size == 0)
        return {};
      else
      {
        using subType = decltype(detail::get_first_type(typeId));
        return subType::enum_name(value, clean);
      }
    }
    // returns the reflected string of an enum with the specified id that is located somewhere in the subtree
    // the algorithm is a top-down DFS (in case you have repeating ids, which is not a good idea)
    static constexpr auto enum_name_by_id_recursive(std::string_view id, bool clean = false) -> std::optional<std::string_view>
    {
      auto value = enum_name_by_id(id, clean);
      if (value.has_value())
        return value;

      if constexpr (E::enumValues.size() == 0)
        return {};
      else
      {
        return []<typename ... Ts>(std::string_view id, bool clean, detail::type_list<Ts...>)
        {
          constexpr auto recurse = []<auto Self, typename U, typename ... Us>(std::string_view id, bool clean)
          {
            auto value = U::enum_name_by_id_recursive(id, clean);
            if (value.has_value())
              return value;

            if constexpr (sizeof...(Us) > 0)
              return Self.template operator()<Self, Us...>(id, clean);
            else
              return std::optional<std::string_view>{};
          };

          return recurse.template operator()<recurse, Ts...>(id, clean);
        }(id, clean, E::subtypes);
      }
    }
    // returns the id of an enum value that is located somewhere in the subtree
    static constexpr auto enum_id_recursive(detail::Enum auto value) -> std::optional<std::string_view>
    {
      auto typeId = find_type_recursive_internal<[]<typename T>()
      {
        return std::is_same_v<decltype(value), typename T::Value>;
      }>(E::subtypes);

      if constexpr (typeId.size == 0)
        return {};
      else
      {
        using subType = decltype(detail::get_first_type(typeId));
        return subType::enum_id(value);
      }
    }
    // returns the id of an enum with the specified reflected string that is located somewhere in the subtree
    static constexpr auto enum_id_recursive(std::string_view enumName) -> std::optional<std::string_view>
    {
      auto value = enum_id(enumName);
      if (value.has_value())
        return value;

      if constexpr (E::enumValues.size() == 0)
        return {};
      else
      {
        return []<typename ... Ts>(std::string_view enumName, detail::type_list<Ts...>)
        {
          constexpr auto recurse = []<auto Self, typename U, typename ... Us>(std::string_view enumName)
          {
            auto value = U::enum_id_recursive(enumName);
            if (value.has_value())
              return value;

            if constexpr (sizeof...(Us) > 0)
              return Self.template operator()<Self, Us...>(enumName);
            else
              return std::optional<std::string_view>{};
          };

          return recurse.template operator()<recurse, Ts...>(enumName);
        }(enumName, E::subtypes);
      }
    }
    // returns the underlying integer of an enum value that is located somewhere in the subtree, specified by its reflected string
    // make sure to provide the full enum name to avoid erroneous results
    template<fixed_string enumName>
    static constexpr auto enum_integer_recursive()
    {
      auto typeId = find_type_recursive_internal<[]<typename T>()
      {
        constexpr auto enumInteger = T::enum_integer(std::string_view(enumName));
        return enumInteger.has_value();
      }>(E::subtypes);

      if constexpr (typeId.size == 0)
        return std::optional<typename E::underlying_type>{};
      else
      {
        using subType = decltype(detail::get_first_type(typeId));
        return subType::enum_integer(std::string_view(enumName));
      }
    }
    // returns the underlying integer of an enum value that is located somewhere in the subtree, specified by its id
    template<fixed_string id>
    static constexpr auto enum_integer_by_id_recursive()
    {
      auto typeId = find_type_recursive_internal<[]<typename T>()
      {
        constexpr auto value = T::enum_integer_by_id(std::string_view(id));
        return value.has_value();
      }>(E::subtypes);

      if constexpr (typeId.size == 0)
        return std::optional<typename E::underlying_type>{};
      else
      {
        using subType = decltype(detail::get_first_type(typeId));
        return subType::enum_integer_by_id(std::string_view(id));
      }
    }
    // returns an enum value that is located somewhere in the subtree, specified by its reflected string
    template<fixed_string enumName>
    static constexpr auto enum_value_recursive()
    {
      auto typeId = find_type_recursive_internal<[]<typename T>()
      {
        constexpr auto value = T::enum_value(std::string_view(enumName));
        return value.has_value();
      }>(E::subtypes);

      if constexpr (typeId.size == 0)
        return std::optional<E>{};
      else
      {
        using subType = decltype(detail::get_first_type(typeId));
        return subType::enum_value(std::string_view(enumName));
      }
    }
    // returns an enum value that is located somewhere in the subtree, specified by its id
    template<fixed_string id>
    static constexpr auto enum_value_by_id_recursive()
    {
      auto typeId = find_type_recursive_internal<[]<typename T>()
      {
        constexpr auto value = T::enum_value_by_id(std::string_view(id));
        return value.has_value();
      }>(E::subtypes);

      if constexpr (typeId.size == 0)
        return std::optional<E>{};
      else
      {
        using subType = decltype(detail::get_first_type(typeId));
        return subType::enum_value_by_id(std::string_view(id));
      }
    }
  };

  template<class E>
  concept NestedEnum = requires { typename E::nested_enum_tag; };

  // helper function for doing recursive iteration over types with enum_subtypes()
  // it is intended for expanding branches without needing to write them manually
  // 1st template parameter is the lambda itself and consequent template parameters are the deduced types
  // Example:
  // 
  // template<nested_enum::NestedEnum T>
  // void do_thing(T value)
  // {
  //   nested_enum::recurse_over_types<[]<auto Self, typename U, typename ... Us>(T value)
  //     {
  //       if constexpr (std::is_same_v<decltype(U::value()), T>)
  //         if (U::value() == value)
  //           do_something_else<typename U::linked_type>();
  //       
  //       if constexpr (sizeof...(Us) > 0)
  //         Self.template operator()<Self, Us...>(value);
  //       else
  //       {
  //         // Uncaught case
  //         assert(false);
  //       }
  //     }>(SomeEnum::enum_subtypes(), value);
  // }
  //
  // keep in mind that as of C++20 only captureless lambdas can be used as non-type template parameters
  // any extra parameters can be passed in as a parameter pack
  template<auto Lambda, typename ... Ts, typename ... Args>
  constexpr auto recurse_over_types(const std::tuple<std::type_identity<Ts>...> &, Args&& ... args)
  {
    return Lambda.template operator()<Lambda, Ts...>(std::forward<Args>(args)...);
  }

  template <typename E, typename P>
  constexpr bool operator==(const nested_enum<E, P> &left, const nested_enum<E, P> &right) noexcept
  {
    return static_cast<const E &>(left).internalValue == static_cast<const E &>(right).internalValue;
  }

  template <typename E, typename P>
  constexpr bool operator==(const nested_enum<E, P> &left, const typename E::Value &right) noexcept
  {
    return static_cast<const E &>(left).internalValue == right;
  }

  template <typename E, typename P>
  constexpr bool operator==(const typename E::Value &left, const nested_enum<E, P> &right) noexcept
  {
    return left == static_cast<const E &>(right).internalValue;
  }
}

#undef TEST_INCLUSIVENESS
#define NESTED_ENUM_INTERNAL_EXPAND(...) NESTED_ENUM_INTERNAL_EXPAND4(NESTED_ENUM_INTERNAL_EXPAND4(NESTED_ENUM_INTERNAL_EXPAND4(NESTED_ENUM_INTERNAL_EXPAND4(__VA_ARGS__))))
#define NESTED_ENUM_INTERNAL_EXPAND4(...) NESTED_ENUM_INTERNAL_EXPAND3(NESTED_ENUM_INTERNAL_EXPAND3(NESTED_ENUM_INTERNAL_EXPAND3(NESTED_ENUM_INTERNAL_EXPAND3(__VA_ARGS__))))
#define NESTED_ENUM_INTERNAL_EXPAND3(...) NESTED_ENUM_INTERNAL_EXPAND2(NESTED_ENUM_INTERNAL_EXPAND2(NESTED_ENUM_INTERNAL_EXPAND2(NESTED_ENUM_INTERNAL_EXPAND2(__VA_ARGS__))))
#define NESTED_ENUM_INTERNAL_EXPAND2(...) NESTED_ENUM_INTERNAL_EXPAND1(NESTED_ENUM_INTERNAL_EXPAND1(NESTED_ENUM_INTERNAL_EXPAND1(NESTED_ENUM_INTERNAL_EXPAND1(__VA_ARGS__))))
#define NESTED_ENUM_INTERNAL_EXPAND1(...) __VA_ARGS__

// first one gets used to define horizonal things while this is used for the depth
#define NESTED_ENUM_INTERNAL_EXPAND_SECOND(...) NESTED_ENUM_INTERNAL_EXPAND_SECOND4(NESTED_ENUM_INTERNAL_EXPAND_SECOND4(NESTED_ENUM_INTERNAL_EXPAND_SECOND4(NESTED_ENUM_INTERNAL_EXPAND_SECOND4(__VA_ARGS__))))
#define NESTED_ENUM_INTERNAL_EXPAND_SECOND4(...) NESTED_ENUM_INTERNAL_EXPAND_SECOND3(NESTED_ENUM_INTERNAL_EXPAND_SECOND3(NESTED_ENUM_INTERNAL_EXPAND_SECOND3(NESTED_ENUM_INTERNAL_EXPAND_SECOND3(__VA_ARGS__))))
#define NESTED_ENUM_INTERNAL_EXPAND_SECOND3(...) NESTED_ENUM_INTERNAL_EXPAND_SECOND2(NESTED_ENUM_INTERNAL_EXPAND_SECOND2(NESTED_ENUM_INTERNAL_EXPAND_SECOND2(NESTED_ENUM_INTERNAL_EXPAND_SECOND2(__VA_ARGS__))))
#define NESTED_ENUM_INTERNAL_EXPAND_SECOND2(...) NESTED_ENUM_INTERNAL_EXPAND_SECOND1(NESTED_ENUM_INTERNAL_EXPAND_SECOND1(NESTED_ENUM_INTERNAL_EXPAND_SECOND1(NESTED_ENUM_INTERNAL_EXPAND_SECOND1(__VA_ARGS__))))
#define NESTED_ENUM_INTERNAL_EXPAND_SECOND1(...) __VA_ARGS__

#define NESTED_ENUM_INTERNAL_PARENS ()
#define NESTED_ENUM_INTERNAL_EMPTY()

#define NESTED_ENUM_INTERNAL_EMPTY_STACK() NESTED_ENUM_INTERNAL_EMPTY4(NESTED_ENUM_INTERNAL_EMPTY4(NESTED_ENUM_INTERNAL_EMPTY4(NESTED_ENUM_INTERNAL_EMPTY4(NESTED_ENUM_INTERNAL_EMPTY))))
#define NESTED_ENUM_INTERNAL_EMPTY4(...) NESTED_ENUM_INTERNAL_EMPTY3(NESTED_ENUM_INTERNAL_EMPTY3(NESTED_ENUM_INTERNAL_EMPTY3(NESTED_ENUM_INTERNAL_EMPTY3(__VA_ARGS__ NESTED_ENUM_INTERNAL_EMPTY))))
#define NESTED_ENUM_INTERNAL_EMPTY3(...) NESTED_ENUM_INTERNAL_EMPTY2(NESTED_ENUM_INTERNAL_EMPTY2(NESTED_ENUM_INTERNAL_EMPTY2(NESTED_ENUM_INTERNAL_EMPTY2(__VA_ARGS__ NESTED_ENUM_INTERNAL_EMPTY))))
#define NESTED_ENUM_INTERNAL_EMPTY2(...) NESTED_ENUM_INTERNAL_EMPTY1(NESTED_ENUM_INTERNAL_EMPTY1(NESTED_ENUM_INTERNAL_EMPTY1(NESTED_ENUM_INTERNAL_EMPTY1(__VA_ARGS__ NESTED_ENUM_INTERNAL_EMPTY))))
#define NESTED_ENUM_INTERNAL_EMPTY1(...) __VA_ARGS__ NESTED_ENUM_INTERNAL_EMPTY

#define NESTED_ENUM_INTERNAL_PAREN_STACK() NESTED_ENUM_INTERNAL_PAREN4(NESTED_ENUM_INTERNAL_PAREN4(NESTED_ENUM_INTERNAL_PAREN4(NESTED_ENUM_INTERNAL_PAREN4(()))))
#define NESTED_ENUM_INTERNAL_PAREN4(...) NESTED_ENUM_INTERNAL_PAREN3(NESTED_ENUM_INTERNAL_PAREN3(NESTED_ENUM_INTERNAL_PAREN3(NESTED_ENUM_INTERNAL_PAREN3(() __VA_ARGS__))))
#define NESTED_ENUM_INTERNAL_PAREN3(...) NESTED_ENUM_INTERNAL_PAREN2(NESTED_ENUM_INTERNAL_PAREN2(NESTED_ENUM_INTERNAL_PAREN2(NESTED_ENUM_INTERNAL_PAREN2(() __VA_ARGS__))))
#define NESTED_ENUM_INTERNAL_PAREN2(...) NESTED_ENUM_INTERNAL_PAREN1(NESTED_ENUM_INTERNAL_PAREN1(NESTED_ENUM_INTERNAL_PAREN1(NESTED_ENUM_INTERNAL_PAREN1(() __VA_ARGS__))))
#define NESTED_ENUM_INTERNAL_PAREN1(...) () __VA_ARGS__

#define NESTED_ENUM_INTERNAL_DEFER(...)  __VA_ARGS__ NESTED_ENUM_INTERNAL_EMPTY_STACK() NESTED_ENUM_INTERNAL_PAREN_STACK()
#define NESTED_ENUM_INTERNAL_DEFER_ADD(id) NESTED_ENUM_INTERNAL_EMPTY NESTED_ENUM_INTERNAL_EMPTY id () ()


// this person is a genius https://stackoverflow.com/a/62984543
// the last macro definition is a little messed up but at least it keeps the naming scheme 
#define NESTED_ENUM_INTERNAL_DEPAREN(X) NESTED_ENUM_INTERNAL_ESC(NESTED_ENUM_INTERNAL_ISH X)
#define NESTED_ENUM_INTERNAL_ISH(...) NESTED_ENUM_INTERNAL_ISH __VA_ARGS__
#define NESTED_ENUM_INTERNAL_ESC(...) NESTED_ENUM_INTERNAL_ESC_(__VA_ARGS__)
#define NESTED_ENUM_INTERNAL_ESC_(...) NESTED_ENUM_INTERNAL_VAN ## __VA_ARGS__
#define NESTED_ENUM_INTERNAL_VANNESTED_ENUM_INTERNAL_ISH


#define NESTED_ENUM_INTERNAL_STRINGIFY_PRIMITIVE(...) #__VA_ARGS__
#define NESTED_ENUM_INTERNAL_STRINGIFY(...) NESTED_ENUM_INTERNAL_STRINGIFY_PRIMITIVE(__VA_ARGS__)
#define NESTED_ENUM_INTERNAL_CONDITIONALLY_CALL_CONCAT(x, suffix, arguments) x##suffix arguments
#define NESTED_ENUM_INTERNAL_CONDITIONALLY_CALL(function, suffix, arguments, ...) NESTED_ENUM_INTERNAL_CONDITIONALLY_CALL_CONCAT(function, __VA_OPT__(suffix), arguments)
#define NESTED_ENUM_INTERNAL_CHOOSE_CALL1(getter, ...) getter(__VA_ARGS__)
#define NESTED_ENUM_INTERNAL_CHOOSE_CALL(getter, options, ...) NESTED_ENUM_INTERNAL_CHOOSE_CALL1(getter, __VA_ARGS__ __VA_OPT__(,) NESTED_ENUM_INTERNAL_DEPAREN(options))
#define NESTED_ENUM_INTERNAL_CHOOSE_CALL_BINARY1(...) NESTED_ENUM_INTERNAL_GET_SECOND_OF_MANY(__VA_ARGS__)
#define NESTED_ENUM_INTERNAL_CHOOSE_CALL_BINARY(options, ...) NESTED_ENUM_INTERNAL_CHOOSE_CALL_BINARY1(__VA_OPT__(,) NESTED_ENUM_INTERNAL_DEPAREN(options))


#define NESTED_ENUM_INTERNAL_VALUE_IF_NOT_TEST(one, ...) one
#define NESTED_ENUM_INTERNAL_VALUE_IF_NOT_TEST_(one, ...) __VA_ARGS__
#define NESTED_ENUM_INTERNAL_VALUE_IF_MISSING(one, ...) NESTED_ENUM_INTERNAL_VALUE_IF_NOT_TEST## __VA_OPT__(_) (one, __VA_ARGS__)

#define NESTED_ENUM_INTERNAL_GET_FIRST_OF_MANY(a1, ...) a1
#define NESTED_ENUM_INTERNAL_GET_SECOND_OF_MANY(a1, a2, ...) a2
#define NESTED_ENUM_INTERNAL_GET_THIRD_OF_MANY(a1, a2, a3, ...) a3
#define NESTED_ENUM_INTERNAL_SKIP_FIRST_OF_MANY(a1, ...) __VA_ARGS__
#define NESTED_ENUM_INTERNAL_GET_THIRD_IF_EXISTS(a1, ...) __VA_OPT__(NESTED_ENUM_INTERNAL_SKIP_FIRST_OF_MANY(__VA_ARGS__))


#define NESTED_ENUM_INTERNAL_FOR_EACH(helper, macro, ...) __VA_OPT__(NESTED_ENUM_INTERNAL_EXPAND(helper(macro, __VA_ARGS__)))

#define NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE(macro, separator, a1, ...)                                                                       \
    macro (NESTED_ENUM_INTERNAL_DEPAREN(a1)) __VA_OPT__(NESTED_ENUM_INTERNAL_DEPAREN(separator))                                                                 \
    __VA_OPT__(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE_AGAIN NESTED_ENUM_INTERNAL_PARENS (macro, separator, __VA_ARGS__))
#define NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE_AGAIN() NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE

#define NESTED_ENUM_INTERNAL_INTERLEAVE_TWO(sequence1, item2, ...)                                                                      \
    (NESTED_ENUM_INTERNAL_GET_TYPE(NESTED_ENUM_INTERNAL_DEPAREN(item2)), NESTED_ENUM_INTERNAL_GET_FIRST_OF_MANY sequence1)            \
    __VA_OPT__(, NESTED_ENUM_INTERNAL_INTERLEAVE_TWO_AGAIN NESTED_ENUM_INTERNAL_PARENS ((NESTED_ENUM_INTERNAL_SKIP_FIRST_OF_MANY sequence1), __VA_ARGS__))
#define NESTED_ENUM_INTERNAL_INTERLEAVE_TWO_AGAIN() NESTED_ENUM_INTERNAL_INTERLEAVE_TWO


#define NESTED_ENUM_INTERNAL_GET_TYPE(...) NESTED_ENUM_INTERNAL_GET_FIRST_OF_MANY(__VA_ARGS__)
#define NESTED_ENUM_INTERNAL_GET_TYPE_STRING(...) NESTED_ENUM_INTERNAL_STRINGIFY(NESTED_ENUM_INTERNAL_GET_FIRST_OF_MANY(__VA_ARGS__))

#define NESTED_ENUM_INTERNAL_PASTE_VAL(value) value, , 
#define NESTED_ENUM_INTERNAL_PASTE_ID(id) , id, 
#define NESTED_ENUM_INTERNAL_PASTE_TYPE(type) , ,type
#define NESTED_ENUM_INTERNAL_PASTE_VAL_ID(value, id) value, id,
#define NESTED_ENUM_INTERNAL_PASTE_VAL_TYPE(value, type) value, , type
#define NESTED_ENUM_INTERNAL_PASTE_ID_TYPE(id, type) , id, type
#define NESTED_ENUM_INTERNAL_PASTE_VAL_ID_TYPE(value, id, type) value, id, type

// __VA_ARGS__ are the expanded typePack from an enum body definition
#define NESTED_ENUM_INTERNAL_GET_VAL_ID_TYPE(functions, getter, name, ...) NESTED_ENUM_INTERNAL_GET_VAL_ID_TYPE1((NESTED_ENUM_INTERNAL_GET_VAL_ID_TYPE_CONTINUE, NESTED_ENUM_INTERNAL_SKIP_FIRST_OF_MANY functions), functions, getter, name, __VA_ARGS__)
#define NESTED_ENUM_INTERNAL_GET_VAL_ID_TYPE1(functions, functions2, getter, name, ...) NESTED_ENUM_INTERNAL_CHOOSE_CALL_BINARY(functions, __VA_ARGS__)(name, functions2, getter, __VA_ARGS__)
#define NESTED_ENUM_INTERNAL_GET_VAL_ID_TYPE_CONTINUE(name, functions, getter, expectedParametersSuffix, ...) NESTED_ENUM_INTERNAL_GET_VAL_ID_TYPE_CONTINUE1(functions, getter, name, NESTED_ENUM_INTERNAL_PASTE_##expectedParametersSuffix(__VA_ARGS__))
#define NESTED_ENUM_INTERNAL_GET_VAL_ID_TYPE_CONTINUE1(functions, getter, name, ...) NESTED_ENUM_INTERNAL_GET_VAL_ID_TYPE_CONTINUE2(functions, name, getter (__VA_ARGS__))
#define NESTED_ENUM_INTERNAL_GET_VAL_ID_TYPE_CONTINUE2(functions, name, ...) NESTED_ENUM_INTERNAL_CHOOSE_CALL(NESTED_ENUM_INTERNAL_GET_SECOND_OF_MANY, functions __VA_OPT__(,) __VA_ARGS__)(name __VA_OPT__(,) __VA_ARGS__)

#define NESTED_ENUM_INTERNAL_GET_ID(...) NESTED_ENUM_INTERNAL_GET_ID1(__VA_ARGS__)
#define NESTED_ENUM_INTERNAL_GET_ID1(name, ...) NESTED_ENUM_INTERNAL_GET_VAL_ID_TYPE((NESTED_ENUM_INTERNAL_GET_ID_FINAL2, NESTED_ENUM_INTERNAL_GET_ID_FINAL1), NESTED_ENUM_INTERNAL_GET_SECOND_OF_MANY, name __VA_OPT__(,) __VA_ARGS__)
#define NESTED_ENUM_INTERNAL_GET_ID_FINAL1(name, ...) {}
#define NESTED_ENUM_INTERNAL_GET_ID_FINAL2(name, ...) __VA_ARGS__

#define NESTED_ENUM_INTERNAL_GET_VAL(...) NESTED_ENUM_INTERNAL_GET_VAL1(__VA_ARGS__)
#define NESTED_ENUM_INTERNAL_GET_VAL1(name, ...) NESTED_ENUM_INTERNAL_GET_VAL_ID_TYPE((NESTED_ENUM_INTERNAL_GET_VAL_FINAL2, NESTED_ENUM_INTERNAL_GET_VAL_FINAL1), NESTED_ENUM_INTERNAL_GET_FIRST_OF_MANY, name __VA_OPT__(,) __VA_ARGS__)
#define NESTED_ENUM_INTERNAL_GET_VAL_FINAL1(name, ...) ::nested_enum::detail::opt<underlying_type>{ false, underlying_type(0) }
#define NESTED_ENUM_INTERNAL_GET_VAL_FINAL2(name, ...) ::nested_enum::detail::opt<underlying_type>{ true, underlying_type(__VA_ARGS__) }

#define NESTED_ENUM_INTERNAL_DEFINE_LINKED_TYPE(...) NESTED_ENUM_INTERNAL_DEFINE_LINKED_TYPE1(__VA_ARGS__)
#define NESTED_ENUM_INTERNAL_DEFINE_LINKED_TYPE1(name, ...) struct name; friend struct name;                                                               \
    private: using name##_linked_type = NESTED_ENUM_INTERNAL_GET_VAL_ID_TYPE((NESTED_ENUM_INTERNAL_DEFINE_LINKED_TYPE_FINAL2, NESTED_ENUM_INTERNAL_DEFINE_LINKED_TYPE_FINAL1), NESTED_ENUM_INTERNAL_GET_THIRD_OF_MANY, name, __VA_ARGS__); public:
#define NESTED_ENUM_INTERNAL_DEFINE_LINKED_TYPE_FINAL1(name, ...) void
#define NESTED_ENUM_INTERNAL_DEFINE_LINKED_TYPE_FINAL2(name, ...) __VA_ARGS__

#define NESTED_ENUM_INTERNAL_DEFINE_ENUM(...) NESTED_ENUM_INTERNAL_DEFINE_ENUM1(__VA_ARGS__)
#define NESTED_ENUM_INTERNAL_DEFINE_ENUM1(name, ...) name NESTED_ENUM_INTERNAL_GET_VAL_ID_TYPE((NESTED_ENUM_INTERNAL_DEFINE_ENUM_FINAL2, NESTED_ENUM_INTERNAL_DEFINE_ENUM_FINAL1), NESTED_ENUM_INTERNAL_GET_FIRST_OF_MANY, name __VA_OPT__(,) __VA_ARGS__)
#define NESTED_ENUM_INTERNAL_DEFINE_ENUM_FINAL1(name, ...) 
#define NESTED_ENUM_INTERNAL_DEFINE_ENUM_FINAL2(name, ...) = __VA_ARGS__

#define NESTED_ENUM_INTERNAL_STRUCT_IDENTITY(typeName) struct typeName

// this is where the magic happens, so i'll write down how it works before i forget
// the way for_each works is through macro deferred expressions and continuous rescanning by EXPAND
// this however not only rescans the current iteration but also all previous ones and everything inside them
// in order to avoid rescanning and substituting the deferred NESTED_ENUM_INTERNAL_SETUP calls we need to defer the initial macro call
// as many times as there are expands - 1, hence the deferStages bit
// but because after every loop iteration one less expand is left we need to also decrease the number of deferrals
// during the process multiple deferrals are exhausted so we actually need to add one
// needs the main for_each macro to pass it the deferStages
#define NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE_WITH_EXTRA_ARGS(macro, deferStages, extraArgs, a1, ...)                                         \
    macro deferStages (extraArgs, a1, __VA_ARGS__)                                                                                            \
    __VA_OPT__(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE_WITH_EXTRA_ARGS_AGAIN NESTED_ENUM_INTERNAL_PARENS (macro, NESTED_ENUM_INTERNAL_DEFER_ADD(deferStages), extraArgs, __VA_ARGS__))
#define NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE_WITH_EXTRA_ARGS_AGAIN() NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE_WITH_EXTRA_ARGS


// extraArgs are currently - (parentName), arg - (typeName, (specialisation specifier, ...))
#define NESTED_ENUM_INTERNAL_STRUCT_PREP(extraArgs, arg, ...) NESTED_ENUM_INTERNAL_STRUCT_PREP1(NESTED_ENUM_INTERNAL_DEPAREN(extraArgs), NESTED_ENUM_INTERNAL_DEPAREN(arg))
#define NESTED_ENUM_INTERNAL_STRUCT_PREP1(parent, ...) NESTED_ENUM_INTERNAL_STRUCT_PREP2(parent, __VA_ARGS__)
#define NESTED_ENUM_INTERNAL_STRUCT_PREP2(parent, typeName, ...) NESTED_ENUM_INTERNAL_STRUCT_PREP3(parent, typeName, NESTED_ENUM_INTERNAL_DEPAREN(__VA_ARGS__))
#define NESTED_ENUM_INTERNAL_STRUCT_PREP3(parent, typeName, treeCall, ...) NESTED_ENUM_INTERNAL_STRUCT_PREP_CALL(parent, typeName, treeCall __VA_OPT__(,) __VA_ARGS__)
#define NESTED_ENUM_INTERNAL_STRUCT_PREP_CALL(parent, typeName, treeCall, ...)  NESTED_ENUM_INTERNAL_TREE_##treeCall(parent, typeName, __VA_ARGS__)


#define NESTED_ENUM_INTERNAL_DEFINE_CONSTRUCTORS(typeName)                                                                                    \
  private:                                                                                                                                    \
    constexpr typeName() noexcept = default;                                                                                                  \
  public:                                                                                                                                     \
    template<::std::size_t N>                                                                                                                 \
    static constexpr auto internal_get_array_of_type() noexcept { return ::std::array<typeName, N>{}; }                                       \
    constexpr typeName(const typeName &other) noexcept = default;                                                                             \
    constexpr typeName(Value value) noexcept { internalValue = value; }                                                                       \
    constexpr typeName &operator=(const typeName &other) noexcept = default;                                                                  \
    constexpr typeName &operator=(Value value) noexcept { internalValue = value; return *this; }

#define NESTED_ENUM_INTERNAL_DEFINITION_NO_PARENT(isLeafValue, placeholder, typeName, ...)                                                    \
  struct typeName : public ::nested_enum::nested_enum<struct typeName, struct typeName>                                                       \
  {                                                                                                                                           \
    using underlying_type = NESTED_ENUM_INTERNAL_VALUE_IF_MISSING(NESTED_ENUM_DEFAULT_ENUM_TYPE,                                              \
      NESTED_ENUM_INTERNAL_GET_FIRST_OF_MANY(__VA_ARGS__));                                                                                   \
    using linked_type = NESTED_ENUM_INTERNAL_VALUE_IF_MISSING(typeName, NESTED_ENUM_INTERNAL_GET_THIRD_IF_EXISTS(__VA_ARGS__));               \
                                                                                                                                              \
    static constexpr auto internalGlobalPrefix = ::nested_enum::detail::get_prefix<                                                           \
      __VA_OPT__(NESTED_ENUM_INTERNAL_SKIP_FIRST_OF_MANY(__VA_ARGS__))>();                                                                    \
    static constexpr bool isLeaf = isLeafValue;                                                                                               \
    static constexpr auto internalName = create_name(::nested_enum::fixed_string{ #typeName });

#define NESTED_ENUM_INTERNAL_DEFINITION_(isLeafValue, parent, typeName, ...)                                                                  \
  struct typeName : public ::nested_enum::nested_enum<struct typeName, struct parent>                                                         \
  {                                                                                                                                           \
    using underlying_type = NESTED_ENUM_INTERNAL_VALUE_IF_MISSING(NESTED_ENUM_DEFAULT_ENUM_TYPE,                                              \
      NESTED_ENUM_INTERNAL_GET_FIRST_OF_MANY(__VA_ARGS__));                                                                                   \
    using linked_type = parent::typeName##_linked_type;                                                                                       \
                                                                                                                                              \
    static constexpr bool isLeaf = isLeafValue;                                                                                               \
    static constexpr auto internalName = create_name(::nested_enum::fixed_string{ #typeName });                                               \
    static constexpr auto value() { struct parent result = parent::typeName; return result; }

#define NESTED_ENUM_INTERNAL_DEFINITION_FROM(isLeafValue, parent, typeName, ...)                                                              \
  struct parent::typeName : public ::nested_enum::nested_enum<struct parent::typeName, struct parent>                                         \
  {                                                                                                                                           \
    using underlying_type = NESTED_ENUM_INTERNAL_VALUE_IF_MISSING(NESTED_ENUM_DEFAULT_ENUM_TYPE,                                              \
      NESTED_ENUM_INTERNAL_GET_FIRST_OF_MANY(__VA_ARGS__));                                                                                   \
    using linked_type = parent::typeName##_linked_type;                                                                                       \
                                                                                                                                              \
    static constexpr bool isLeaf = isLeafValue;                                                                                               \
    static constexpr auto internalName = create_name(::nested_enum::fixed_string{ #typeName });                                               \
    static constexpr auto value() { struct parent result = parent::typeName; return result; }


#define NESTED_ENUM_INTERNAL_BODY_(typeName, ...)                                                                                             \
                                                                                                                                              \
    enum Value : underlying_type { };                                                                                                         \
    Value internalValue;                                                                                                                      \
                                                                                                                                              \
    NESTED_ENUM_INTERNAL_DEFINE_CONSTRUCTORS(typeName)                                                                                        \
                                                                                                                                              \
    constexpr operator Value() const noexcept { return internalValue; }                                                                       \
                                                                                                                                              \
    static constexpr ::std::array<Value, 0> enumValues{};                                                                                     \
                                                                                                                                              \
    static constexpr ::std::array<::std::optional<std::string_view>, 0> enumIds{};                                                            \
                                                                                                                                              \
    static constexpr ::nested_enum::detail::type_list<> subtypes{};                                                                           \
  };

#define NESTED_ENUM_INTERNAL_BODY_CONTINUE(typeName, bodyArguments, structsArguments)                                                         \
                                                                                                                                              \
    enum Value : underlying_type { NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE,                                       \
      NESTED_ENUM_INTERNAL_DEFINE_ENUM, (,), NESTED_ENUM_INTERNAL_DEPAREN(bodyArguments)) };                                                  \
    Value internalValue;                                                                                                                      \
                                                                                                                                              \
    NESTED_ENUM_INTERNAL_DEFINE_CONSTRUCTORS(typeName)                                                                                        \
                                                                                                                                              \
    constexpr operator Value() const noexcept { return internalValue; }                                                                       \
                                                                                                                                              \
    static constexpr auto enumValues = ::nested_enum::detail::get_array_of_values<Value, underlying_type,                                     \
      NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE,                                                                    \
        NESTED_ENUM_INTERNAL_GET_VAL, (,), NESTED_ENUM_INTERNAL_DEPAREN(bodyArguments))>();                                                   \
                                                                                                                                              \
    static constexpr auto enumNames = ::nested_enum::detail::get_string_values<internalName,                                                  \
      NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE, NESTED_ENUM_INTERNAL_GET_TYPE_STRING, (,),                         \
        NESTED_ENUM_INTERNAL_DEPAREN(bodyArguments))>();									                                                                    \
                                                                                                                                              \
    static constexpr auto enumIds = ::std::to_array<::std::optional<::std::string_view>>                                                      \
      ({ NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE, NESTED_ENUM_INTERNAL_GET_ID, (,),                               \
        NESTED_ENUM_INTERNAL_DEPAREN(bodyArguments)) });                                                                                      \
                                                                                                                                              \
    NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE, NESTED_ENUM_INTERNAL_DEFINE_LINKED_TYPE, (),                         \
      NESTED_ENUM_INTERNAL_DEPAREN(bodyArguments))                                                                                            \
                                                                                                                                              \
    NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE_WITH_EXTRA_ARGS, NESTED_ENUM_INTERNAL_STRUCT_PREP,                    \
      NESTED_ENUM_INTERNAL_DEFER_ADD(NESTED_ENUM_INTERNAL_DEFER()), (typeName),                                                               \
      NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_INTERLEAVE_TWO, structsArguments, NESTED_ENUM_INTERNAL_DEPAREN(bodyArguments)))      \
                                                                                                                                              \
    static constexpr ::nested_enum::detail::type_list<NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE,                    \
      NESTED_ENUM_INTERNAL_STRUCT_IDENTITY, (,), NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE,                         \
      NESTED_ENUM_INTERNAL_GET_TYPE, (,), NESTED_ENUM_INTERNAL_DEPAREN(bodyArguments)))> subtypes{};                                          \
  };


#define NESTED_ENUM_INTERNAL_SETUP(definitionType, definitionArguments, ...)                                                                  \
  NESTED_ENUM_INTERNAL_DEFINITION_##definitionType definitionArguments                                                                        \
  NESTED_ENUM_INTERNAL_CONDITIONALLY_CALL(NESTED_ENUM_INTERNAL_BODY_, CONTINUE,                                                               \
    (NESTED_ENUM_INTERNAL_GET_THIRD_OF_MANY definitionArguments, NESTED_ENUM_INTERNAL_GET_FIRST_OF_MANY(__VA_ARGS__),                         \
      (NESTED_ENUM_INTERNAL_SKIP_FIRST_OF_MANY(__VA_ARGS__))), __VA_ARGS__)

#define NESTED_ENUM_INTERNAL_SETUP_AGAIN() NESTED_ENUM_INTERNAL_SETUP

// defines an outer node inside a tree
#define NESTED_ENUM_INTERNAL_TREE_(parent, typeName, ...) NESTED_ENUM_INTERNAL_SETUP_AGAIN NESTED_ENUM_INTERNAL_PARENS (, (true, parent, typeName))
// defines an inner node inside a tree
#define NESTED_ENUM_INTERNAL_TREE_ENUM(parent, typeName, enumDefinition, ...) NESTED_ENUM_INTERNAL_SETUP_AGAIN NESTED_ENUM_INTERNAL_PARENS (, (false, parent, NESTED_ENUM_INTERNAL_DEPAREN(enumDefinition)), __VA_ARGS__)
// only forward declares a node because the user wants to define it later 
#define NESTED_ENUM_INTERNAL_TREE_DEFER(parent, typeName, ...) 

//==================================================================================

// defines a (root) enum that can be expanded into a tree
// the macro can be divided into 3 parts: definition, entries, specialisations
// 
//                    definition                                                                           entries                                                                 specialisations
//                        |                                                                                   |                                                                           |
//                        V                                                                                   V                                                                           V
// (name, [underlying type], [global string prefix], [linked type]), ((enum value name, [extra input specifier], [extra inputs]), enum values...), (specialisation specifier, child definition, child entries, child specialisations), ...
//                                      ^                                                                                                          \____________________________________  __________________________________________/  \ /
//                                      |                                                                                                                                               \/                                              V    
//                        (present only in topmost enum)                                                                                                                            (1st child)                              (consecutive children)
// 
// If a section closed by parentheses has only 1 argument, the parentheses can be omitted
// 
// Example:
// 
//    NESTED_ENUM((Vehicle, std::uint32_t, "Category"), (Land, Watercraft, Amphibious, Aircraft),
//      (ENUM, (Land, std::uint64_t), (Motorcycle, Car, Bus, Truck, Tram, Train), 
//        (DEFER),
//        (ENUM, Car, ((Minicompact, ID, "A-segment"), (Subcompact, ID, "B-segment"), (Compact, ID,
//          "C-segment"), (MidSize, ID, "D-segment"), (FullSize, ID, "E-segment"), (Luxury, ID, "F-segment")),
//          (ENUM, Minicompact, ((Fiat_500, TYPE, Fiat_type), (Hyundai_i10, TYPE, Hyundai_type), (Toyota_Aygo, TYPE, Toyota_type), ...)),
//          (ENUM, Subcompact, (Chevrolet_Aveo, Hyundai_Accent, Volkswagen_Polo, ...))
//        ),
//        (ENUM, Bus, (Shuttle, Trolley, School, Coach, Articulated)),
//        ...
//      )  
//    )
//    
//    NESTED_ENUM_FROM(Vehicle::Land, Motorcycle, (Scooter, Cruiser, Sport, OffRoad))
//
#define NESTED_ENUM(enumDefinition, ...) NESTED_ENUM_INTERNAL_EXPAND_SECOND(NESTED_ENUM_INTERNAL_SETUP(NO_PARENT, (false, (), NESTED_ENUM_INTERNAL_DEPAREN(enumDefinition)), __VA_ARGS__))

// defines a deferred enum from a tree definition; needs to fill out the parent explicitly
// see NESTED_ENUM macro for more information
#define NESTED_ENUM_FROM(parent, enumDefinition, ...) NESTED_ENUM_INTERNAL_EXPAND_SECOND(NESTED_ENUM_INTERNAL_SETUP(FROM, (false, parent, NESTED_ENUM_INTERNAL_DEPAREN(enumDefinition)), __VA_ARGS__))


#endif
