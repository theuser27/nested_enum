# nested_enum

Header-only C++20 library for enums that support nesting, name reflection and identification through other strings 

Currently supported minimum compiler versions: gcc 11.1, clang 13.0.0, msvc 19.30

---
To start off, just declare your enum with the `NESTED_ENUM` macro, where your first entry is the enum name and the following entries are the enum values

```c++
NESTED_ENUM(Food, Meat, Vegetables, Fruits)

	NESTED_ENUM(Food::Meat, Beef, Poultry, Venison)

		NESTED_ENUM(Food::Meat::Beef, Rare, Medium Rare, Medium, Medium Well, Well Done)

	NESTED_ENUM(Food::Vegetables, Tomato, Potato, Carrot, Cucumber)
```
You don't have to indent things the same way (or even declare them in this order) but it makes it easier to see what goes where

If you want to identify your enum values with a string/id (other than the reflected one), you can use the `NESTED_ENUM_WITH_IDS` macro where you declare your ids for every enum value. And in case you don't want/need to address a specific enum value with a string, you can always pass in `""` or `{}`

```c++
NESTED_ENUM(Food, Meat, Vegetables, Fruits)

	NESTED_ENUM_WITH_IDS(Food::Meat, Beef, "quite tasty", Poultry, {}, Venison, "not as tasty")

		NESTED_ENUM(Food::Meat::Beef, Rare, Medium Rare, Medium, Medium Well, Well Done)

	NESTED_ENUM_WITH_IDS(Food::Vegetables, Tomato, "for pizza", Potato, "for soup" Carrot, Cucumber, "for salad")
```
Unfortunately if you decide to declare IDs for enum values you have to declare one for every one of them currently 

TODO: add more things here
