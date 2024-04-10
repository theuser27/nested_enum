# nested_enum

Header-only fully-constexpr zero-dependency C++20 library for nestable enums with name reflection and string identification 

Currently tested working compiler versions: gcc 10.1, clang 13.0.0, msvc 19.30 (needs /Zc:preprocessor), but technically any C++20 compiler with a conformant preprocessor should work (**does not rely** on compiler specific features like `__PRETTY_FUNCTION__` or `FUNCSIG`)

---
To start off, create your enum with the `NESTED_ENUM` macro
```c++
class Car_t;
class Fiat_t;
class Hyundai_t;
class Toyota_t;

NESTED_ENUM((Vehicle, std::uint32_t, "Category"), (Land, Watercraft, Amphibious, Aircraft),
  (ENUM, (Land, std::uint64_t), (Motorcycle, Car, Bus, Truck, Tram, Train), 
    (DEFER),
    (ENUM, (Car, std::uint64_t, Car_t),
      (
        (Minicompact, VAL_ID, 10, "A-segment"), (Subcompact, VAL_ID, 20, "B-segment"),
        (Compact    , VAL_ID, 30, "C-segment"), (MidSize   , VAL_ID, 40, "D-segment"), 
        (FullSize   , VAL_ID, 50, "E-segment"), (Luxury    , VAL_ID, 60, "F-segment")
      ),
      (ENUM, Minicompact,
        (
          (Fiat_500   , ID_TYPE, "1st", Fiat_t), 
          (Hyundai_i10, ID_TYPE, "2nd", Hyundai_t), 
          (Toyota_Aygo, ID_TYPE, "3rd", Toyota_t))
        ),
      (ENUM, Subcompact, (Chevrolet_Aveo, Hyundai_Accent, Volkswagen_Polo))
    ),
    (ENUM, Bus, (Shuttle, Trolley, School, Coach, Articulated))
  )  
)

NESTED_ENUM_FROM(Vehicle::Land, Motorcycle, (Scooter, Cruiser, Sport, OffRoad))
```
You might be wondering "Ok, what's going on here?". No worries, the macro syntax is fairly simple. Every enum has 3 comma separated sections (definition, entries, specialisations), where all parameters in [] are optional: 

`(name, [underlying type], [global string prefix], [linked type])` , `((enum value name, [extra input specifier], [extra inputs...]), enum values...)` , `(specialisation specifier, child definition, child entries, child entries' specialisations...), more entry specialisations...)`

 - [underlying type] - any integral type that can be used for a normal enum/enum class. By default int64_t is used if underlying type is not specified 

 - [extra input specifier + extra inputs] - `VAL`/`ID`/`TYPE`/`VAL_ID`/`VAL_TYPE`/`ID_TYPE`/`VAL_ID_TYPE`, these specifiers designate how the following inputs are going to be interpreted: `VAL` - assigns a numeric value to the enum entry, `ID` - allows you to specify an extra string you can identify the entry by, `TYPE` - specifies a supplementary type that can be accessed under *`enum name`***::**`linked_type`
 - specialisation specifier - *`blank`*/`ENUM`/`DEFER`, defaulting an sub-enum / immediately creating a given sub-enum / only forward declaring one that will be created by the user with `NESTED_ENUM_FROM` (when specialising the order of the names must be the same as in the entry section)

The following parameters are only present for the topmost enum type
 - [global prefix] - a string literal that you can prepend to all names under this nested_enum (can used to add namespace name or any other identifier in front)
 - [linked type] - does the same as specifing a `TYPE` for a value but for the topmost enum type

### Miscelaneous Information
 * If an enum value doesn't need to be specialised simply passing in `()` will explicitly default it. If any of the sections have only one argument, the parentheses can be omitted
 * If not specified, the default linked_type is `void`
 * Enums are not default constructible in order to avoid situations where 0 is not a valid enum value
 * Because enums are just structs, they can be forward declared

## Function Examples
 - Basic enum/reflection operations

	```c++
	std::string_view name = Vehicle::name(false); 
	// "Category::Vehicle"
	std::optional<std::string_view> id = Vehicle::Land::Car::Subcompact::id();
	// "B-segment"

	Vehicle::type amphibious = Vehicle(Vehicle::Amphibious);
	// Vehicle{ value == Vehicle::(internal enum type)::Amphibious }
	std::string_view amphibiousString = amphibious.enum_name(true);
	// "Amphibious"
	auto amphibiousValue = amphibious.enum_value();
	// Vehicle::(internal enum type)::Amphibious

	Vehicle::type watercraft = Vehicle::make_enum(1);
	// Vehicle{ value == Vehicle::(internal enum type)::Watercraft }
	std::optional<std::string_view> watercraftId = watercraft.enum_id();
	// std::nullopt
	uint32_t watercraftInteger = watercraft.enum_integer();
	// uint32_t(1)
	```

 - Direct switch support
	```c++
	Vehicle::type get_vehicle();
	// ...
	auto vehicle = get_vehicle();
	switch (vehicle)
	{
	case Vehicle::Land:
		// ...
		break;
	case Vehicle::Watercraft:
		// ...
		break;
	case Vehicle::Amphibious:
		// ...
		break;
	case Vehicle::Aircraft:
		// ...
		break;
	}
	```

 - Batch operations
	```c++
	auto vehicleValues = Vehicle::enum_values();
	// std::array<Vehicle::(internal enum type), 4>{ Land, Watercraft, Amphibious, Aircraft }
	std::size_t outerVehicleValuesCount = Vehicle::enum_count(nested_enum::OuterNodes);
	// 3
	auto outerVehicleTypes = Vehicle::enum_subtypes<nested_enum::OuterNodes>();
	// std::tuple<std::type_identity<struct Vehicle::Watercraft>, 
	//            std::type_identity<struct Vehicle::Amphibious>, 
	//            std::type_identity<struct Vehicle::Aircraft>>{}
	auto innerVehicleStrings = Vehicle::enum_names<nested_enum::InnerNodes>(false);
	// std::array<std::string_view, 1>{ "Category::Vehicle::Land" }
	auto minicompactCarIds = Vehicle::Land::Car::Minicompact::enum_ids();
	// std::array<std::string_view, 3>{ "1st", "2nd", "3rd" }
	auto minicompactCarStringsAndIds = Vehicle::Land::Car::Minicompact::enum_names_and_ids();
	// std::array<std::pair<std::string_view, std::string_view>, 3>{ 
	//   { "Category::Vehicle::Land::Car::Minicompact::Fiat_500"   , "1st" }, 
	//   { "Category::Vehicle::Land::Car::Minicompact::Hyundai_i10", "2nd" }, 
	//   { "Category::Vehicle::Land::Car::Minicompact::Toyota_Aygo", "3rd" } }
	```

 - Recursive operations
	```c++
	std::size_t allCategoriesCount = Vehicle::enum_count_recursive(nested_enum::InnerNodes);
	// 6
	auto allCarEnumValues = Vehicle::Land::Car::enum_values_recursive<nested_enum::AllNodes>();
	// std::tuple<std::array<Vehicle::Land::Car::(internal enum type), 6>,
	//            std::array<Vehicle::Land::Car::Minicompact::(internal enum type), 3>,
	//            std::array<Vehicle::Land::Car::Subcompact::(internal enum type), 3>>
	auto allCarEnumStrings = Vehicle::Land::Car::enum_names_recursive<nested_enum::OuterNodes>();
	// std::array<std::string_view, 10>{  
	//   "Vehicle::Land::Car::Compact", "Vehicle::Land::Car::MidSize", 
	//   "Vehicle::Land::Car::FullSize", "Vehicle::Land::Car::Luxury", 
	//   "Vehicle::Land::Car::Minicompact::Fiat_500",
	//   "Vehicle::Land::Car::Minicompact::Hyundai_i10",
	//   "Vehicle::Land::Car::Minicompact::Toyota_Aygo", 
	//   "Vehicle::Land::Car::Subcompact::Chevrolet_Aveo",
	//   "Vehicle::Land::Car::Subcompact::Hyundai_Accent", 
	//   "Vehicle::Land::Car::Subcompact::Volkswagen_Polo" }
	```

 - Inspection operations
	```c++
	auto motorcycleEnumString = Vehicle::Land::enum_name(Vehicle::Land::Motorcycle, true);
	// "Motorcycle"
	auto CSegmentCar = Vehicle::enum_integer_recursive_by_id<"C-segment">();
	// std::uint64_t(30)
	auto fullSizeCarValue = Vehicle::enum_value_recursive<"Category::Vehicle::Land::Car::FullSize">();
	// Vehicle::Land::Car{ value = Vehicle::Land::Car::(internal enum type)::FullSize }
	```

## Caveats
 1. Unfortunately most intellisense engines will give up trying to expand all of the macro soup involved and will report false positives at definition sites, and if you nest too much in a single `NESTED_ENUM` macro (from my experience more than 2 levels) autocomplete may also cease to see types at those levels. If this is an issue it's recommended to `DEFER` nested definitions and using `NESTED_ENUM_FROM` macro to define them underneath the parent one

 2. Because this library utilises regular old C enums, the enum values themselves overshadow the created structs, therefore you have to use an elaborated type specifier (prepend `struct`) or the `::type` type alias helper if you need to specify the type explictly (i.e. inside a function parameter list)
	```c++
	//void do_thing(Vehicle::Land thing);      // error: compiler sees Vehicle::(internal enum type)::Land
	void do_thing(Vehicle::Land::type thing);  // ✔
	void do_thing(struct Vehicle::Land thing); // ✔

	int main()
	{
	  do_thing(Vehicle::Land::Car);
	}
	```


