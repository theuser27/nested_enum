# nested_enum

Header-only C++20 library for enums that support nesting, name reflection and identification through other strings 

Currently supported minimum compiler versions: gcc 11.1, clang 13.0.0, msvc 19.30 (needs /Zc:preprocessor)

---
To start off, create your enum with the `NESTED_ENUM` macro
```c++
NESTED_ENUM((Food, int), (, Meat, Vegetables, Fruits)
	(ENUM, (Meat, uint64_t), (IDS, Beef, "quite tasty", Poultry, {}, Venison, "not as tasty"),
		(ENUM, Beef, (, Rare, Medium Rare, Medium, Medium Well, Well Done))
	),
	(ENUM, Vegetables, (, Tomato, Potato, Carrot, Cucumber))
)
```
You might be wondering "Ok, what's going on here?". No worries, the macro syntax is fairly simple. Every enum has 3 comma separated sections (definition, entries, specialisations), where all parameters in [] are optional: 

`(name, [underlying type]),` `([extra input specifier], enum values...),` `(specialisation specifier, ` *`child definition, child entries, child entries' specialisations...`*`), ` *`more entry specialisations...`*`)`

 - [underlying type] - any integral type that can be used for a normal enum/enum class
 - [extra input specifier] - currently *`blank`*/`IDS`, the former is for simply entering enum values while the latter allows you to specify extra strings you can identify them with after every entry
 - [specialisation specifier] - currently `ENUM`/`DEFER`, for immediately creating a given sub-enum and only forward declaring one that will be created by the user with `NESTED_ENUM_FROM` respectively (when specialising it is very important that it be in the same order as in the entry section)

If any of the sections have only one argument the parentheses can be omitted

TODO: add more things here
