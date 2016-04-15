                        SuperEasyJSON
            http://www.sourceforge.net/p/supereasyjson
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

PURPOSE:
========

Why write another C++ JSON library? I was getting frustrated
with the other offerings for a variety of reasons. 

1. I didn't want to install Boost or any other library 
2. I didn't want to install any programs/scripts that I didn't already have just to
    create the library (I'm looking at you flex and bison and python)
3. I didn't want to have to run 'make'
4. I just wanted a header and cpp file that I could drop into
    any project and it would work. 
5. I wanted an interface that was easy to use

This library is easy to use, requires no installation (other than adding
it into your project), and doesn't depend on anything else.

It is NOT the library for you if you demand extreme speed or amazing
memory performance. There are already other libraries out there if that's
what you need. For my purposes, I'm neither creating gigantic JSON objects
nor am I making lots of them nor am I making them in some super critical
time-dependent block of code that any of this would have any effect
whatsoever. As an iOS game programmer, I don't think I'd ever be in this
position. 

Could it have been written more efficiently? Of course. But for my
needs, the performance cost of the code as-is is negligible, and the
trade-off was I was able to write and test it very quickly. If
I needed a high performance implementation I'd have used an existing
library or written things completely differently.

So if you want a brain-dead simple library to get started with, and
you can live with the performance trade-offs, then give it a shot.

Any bugs, please email me at my gmail account: jeff.weinstein.
Please use the subject-line "EasyJSON" followed by whatever else. 
Preferably not profanity.

INSTALLATION:
============
Drop json.h and json.cpp into your project. Seriously, that's it.

USAGE:
======
Just include "json.h" and you're ready to go. All classes are in the json
namespace. Examples that follow will assume you are familiar with JSON.
If you aren't, please consult http://www.json.org first as redundant info
won't be repeated. 

CREATING A JSON DATA STRUCTURE: ARRAYS
=======================================
Creating a JSON data structure is easy. According to the JSON specification,
JSON is built on either an array or an object structure. Let's walk through
an array first as it's simpler.

The json::Array class is designed to be simple and should already be
familiar to anyone that has used a std::vector before. You create an array
as such:

json::Array my_array;

Boom. Easy. Now you have to fill it with things (actually, it could be empty,
but that's a boring example). You have 2 ways to add new values. If we just
want to append a value to the end of the array, we would simply call:

my_array.push_back(value)

If, however, we wanted to insert a value at a given index, we would do:
my_array.insert(position, value)

where "position" is an integer from 0 to the size of the array minus 1.
Again, this is just like std::vector's push_back and insert methods.
We'll get into what "Value" is in more detail briefly.

You can access an element of an array just like you would in a std::vector
or normal array. Simply do:

// getting a value
Value my_val = my_array[index];

// overwriting a value
my_array[index] = some_value;

There are begin and end iterators provided if you prefer to use them over
integer indexing. This also allows you to use new C++11 style for loops with
an array class. For example, you could do this in C++11 to iterate over all 
elements easily:

for (auto& my_value : my_array)
	...

Alternatively, to iterate you could do:

for (size_t i = 0; i < my_array.size(); i++)
	...

or even:

for (json::Array::ValueVector::iterator it = my_array.begin(); it != my_array.end(); ++it)
	...

And of course there's a const_iterator as well. Which you use is up to you.

You can search for a value in the array by calling my_array.find(...) and passing
in the value you're looking for. It will return an iterator which can be
dereferenced for the value (just like std::vector.find).

You can check to see if a value is in the array at all by calling HasValue(...)
and passing in the value you're looking for.

To empty the array and revert it back to its default state, call Clear().

WHAT ARE VALUES?
================
No, this is not a morality lesson. This is a good time to discuss the json::Value 
class as it is used in both arrays and objects. As mentioned on the json.org page,
a value is either a string, number, object, array, boolean, or null.

The Value class was designed to be easy to use, allowing you to use normal values
without always having to construct a json::Value class. Let's start with some
simple examples. Below, we'll create a value of one of each of the types (except array/object):

json::Value int_val = 1;
json::Value float_val = 1.0f;
json::Value double_val = 1.0;
json::Value string_val = "string value";
json::Value bool_val = false;
json::Value null_val;

Anywhere a method takes a Value object, you can just enter the value naturally without having to
make a json::Value instance. Since I just said "value" 3 times in a sentence, let's just show
some examples. These examples assume we have created a variable:

json::Array my_array;

//Appending the number 1 to an array:
my_array.push_back(1);

//Appending the float 100.0f:
my_array.push_back(100.0f);

// Appending the string "a magic string":
my_array.push_back("a magic string");

// Seeing if the array contains the number 100.0f:
bool has_it = my_array.HasValue(100.0f); // will be true

// overwriting the value at index 1 with another value:
my_array[1] = 123456;

You hopefully get the idea: the underlying value (string, number, etc) can just simply be
used wherever you see a Value object required. 

The Value class also contains some helper functions. As mentioned above, because a Value can 
actually be an object or array as well, the [] operator has been overridden so that you can 
index just like you do with an array or object (we'll cover objects in the next section).

Because the Value class provides type casting operator overrides, you can easily
convert a Value into the underlying type. For example:

json::Value int_val = 1;
int an_integer = int_val;

json::Value float_val = 1.0f;
float a_float = float_val;

json::Value double_val = 1.0;
double a_double = double_val;

json::Value bool_val = false;
bool a_boolean = bool_val;

json::Value string_val = "string value";
std::string a_string = string_val.ToString();

WHOAH wait a minute, what's with that last one? That doesn't look as convenient. Well the problem
is that as per C++ rules, you can't implicitly cast a Value into a std::string since we already
have functions to implicitly cast to other types. Because we have int/bool/float/double, 
the compiler thinks it's ambiguous as to which cast it's supposed to use. Annoying? Yes.
Can't do anything about it though, sadly. Your 3 choices for converting a Value to a std::string
are:

std::string a_string = string_val.ToString();
std::string a_string2 = (std::string)string_val;
std::string a_string3 = string_val.ToString("a default value");

What's with that last form? That leads me to the To* methods. Besides implicitly casting,
you can also call ToInt(), ToFloat(), ToDouble(), ToBool(), ToString(), ToObject(), and ToArray()
to get the underlying value. There's a form of each of those (except object/array) that will
take a default value in the event that an error ocurred and you tried to cast from an incompatible type.
For example, calling ToInt() on a boolean type will throw an error. Calling ToInt(1234) on a 
boolean type will just return 1234, the default you specified as the parameter, instead of throwing
an error.

You can reset the state of the Value class to its default (which represents a NULL value type)
by calling Clear().

CREATING A JSON DATA STRUCTURE: OBJECTS
=======================================
Now that we've covered arrays and values, let's move on to the final topic, objects.
An object is just a pair of key/values. It's like a map. In fact, drum roll please,
the json::Object class is designed to feel like a std::map. Most of the functions
should be familiar to you after reading the section on arrays. We can insert a new
value into an Object like so:

json::Object my_object;
my_object["new entry"] = "a new string value";

Here, "new entry" is the key and "a new string value" is the value. As you will recall
from the previous section on the Value class, you can simply use the underlying data type
in place of a constructed Value object. You can however, use a Value object, like so:

json::Value string_value("this is a string value");
my_object["new entry"] = string_value;

Same result, although you'll agree the first example is more convenient.

Overwriting an existing entry is easy, you just do the same thing as inserting a new value,
using of course an existing key name. Let's run through some examples for getting the value back:

json::Object my_object;
my_object["string value"] = "this is a string value";
my_object["integer value"] = 123456;
my_object["bool value"] = false;
bool b = my_object["bool value"];
int i = my_object["integer value"];

// Remember that in the previous section we discussed why C++ doesn't allow implicitly casting
// a std::string in this case. 
std::string str = (std::string)my_object["string value"];	

You can call the find(...) method to get an iterator (or const_iterator) for the 
specified key. Just like a std::map, if it's not found it will be equivalent to the end()
iterator. Objects use a ValueMap::iterator. Here's a few examples:

// iterating over the entire object:
for (Object::ValueMap::iterator it = my_object.begin(); it != my_object.end(); ++it)
	...

// Getting an iterator for a key:
Object::ValueMap::iterator it = my_object.find("string value");
if (it != my_object.end())
{
	// we've found the object (note this is continuing from the previous example where we
	// defined "string value" as having, well, a string value. Depending on your needs, you
	// may need to check the type of the value. In this case, we know it's a string.
	printf("Key: %s, Value: %s", it->first.c_str(), it->second.ToString().c_str());
}

As you can see, the ->first type is a std::string and the ->second type is a Value class.

You can check to see if a key exists by calling HasKey. You can check to see if a vector
or array of keys exists as well by calling HasKeys.

SLIGHTLY MORE ADVANCED USAGE TOPICS:
====================================
So now we know what Value, Object, and Array classes are. You also know that a Value, according
to JSON, can represent an Object or Array as well. The Value class contains constructors that
will take an Array or Object type, so that you could do this for example:

json::Object an_object;
json::Array an_array;
json::Value value_obj(an_object);
json::Value value_array(an_array);

// Since value_array represents the Array class, we can call ToArray() on it:
json::Array array2 = value_array.ToArray();		

// And the same idea for Object:
json::Object object2 = value_obj.ToObject();

Now, since value_array is representing an Array class type, we could actually index into it, 
just like we do with the Array class like so:

json::Value value_at_index_1 = value_array[1];
// or overwrite the value:
value_array[1] = "a new value";

Yes, as per JSON specification, an array can contain mixed types. UNLIKE a std::vector, which can only
hold one type, the Array class can hold a mixture of numbers, booleans, strings, NULL, and even more
arrays and objects. So this, for example is valid:

an_array.push_back(1);
an_array.push_back("a string");
an_array.push_back(json::Value());	// this appends a NULL value 
an_array.push_back(another_array);
an_array.push_back(an_object);

And, as you can see, since a Value can be representing an Array type (or Object), for convenience there's a
size() method that will tell you the number of objects contained within. This is always 1 for non arrays/objects.

What about our value_obj example? Well the index operator [] works exactly the same as we explained for
objects. Also, as per JSON specification, because an Object is just a key:Value pairing, and
because we know a Value can also be an array or an object, Objects can contain other objects (or arrays)
in addition to the basic data types. Thus, like with an array, you can have deeply nested sub objects and
sub arrays.

Let's demonstrate a more complex example now that we know a lot more:

	json::Object sub_obj;
	sub_obj["string"] = "sub_obj's string";

	json::Array sub_array;
	sub_array.push_back("sub_array string 1");
	sub_array.push_back("sub_array string 2");
	sub_array.push_back(1.234f);

	json::Array a;
	a.push_back(10);
	a.push_back(10.0f);
	a.push_back(false);
	a.push_back("array a's string");
	a.push_back(sub_obj);
	a.push_back(sub_array);
	// Notice how above our array contains another array (sub_array) and an object (sub_obj).
	// This is completely valid.

	json::Object sub_obj3;
	sub_obj3["sub_obj3 string"] = "whoah, I'm sub_obj3's string";
	sub_obj3["sub_obj3 int"] = 123456;

	json::Object sub_obj2;
	sub_obj2["int"] = 1000;
	sub_obj2["bool"] = false;
	sub_obj2["array value"] = sub_array;
	sub_obj2["object value"] = sub_obj3;
	// Note how this object contains an array (sub_array) and an object (sub_obj3)
	// in addition to the basic data types.

	json::Object obj;
	obj["int"] = 1;
	obj["float"] = 1.1f;
	obj["bool"] = true;
	obj["string"] = "This is a string, hooray";
	obj["array"] = a;
	obj["sub_obj2"] = sub_obj2;
	obj["null value"] = json::Value();
	// As you can see, this object contains many nested objects. The array, "a", we created
	// above itself contained an object and an array. And the object, "sub_obj2" contained
	// an array and an object.

SERIALIZATION: HAS NOTHING TO DO WITH BREAKFAST
===============================================
Because, let's be honest, that would be spelled cerealization. OK so now that we've covered how to make
a JSON data structure, how do we get a string representation of it? And why would you want to do this?
Well as for why, there's many reasons. Maybe a database stores its information as a JSON string, 
maybe some RESTful API requires it, maybe you just want an easy way to save your huge JSON data
structure to disk so you can restore it later. As for how? It's super easy. All you do is this:

std::string serialized_json_string = json::Serialize(my_json_blob)

Hey wait, what's "my_json_blob"? Well, we know from the JSON standard that a JSON data structure,
sometimes just called a JSON blob, must be either an array or an object. We also know that a value,
and the json::Value class, can also be an object or an array. Since the Serialize method is defined like this:

std::string Serialize(const Value& obj);

We can simply pass in a json::Array or json::Object to the method, and it will spit out a string
that represents our JSON data structure. 

IN THE EVENT OF AN ERROR, Serialize will return an empty string. 

Hey wait, what happens if I just pass in a json::Value? Well, that's not valid JSON and you'll get
an empty string back from the call to Serialize. As we just said, according to JSON a valid 
JSON data structure is either an array or an object. Well why didn't you just define two methods, like:

std::string Serialize(const Object& obj);
std::string Serialize(const Array& array);

Well because as you'll recall, a value in JSON can actually refer to an object or an array.
So we could have a situation like this:

json::Object my_obj;
my_obj["a key"] = 12345;

json::Value really_an_object(my_obj);

std::string json_string = json::Serialize(really_an_object);

That should be (and is) perfectly valid.

Soooo great, we have a std::string that represents our JSON data structure. Let's say we just saved that
to disk and sometime later we're reading it and want to re-create a nice, easy to use JSON data structure.
Or perhaps we just got some data back from a web call that gave us a JSON string and we want to
represent it as an array or object for ease of use.

Simple! All you do is this:

json::Value my_data = json::Deserialize(some_string);

In the event that the string you passed in is not valid JSON, my_data will have its
type be NULL. So to check for error, just do:

if (my_data.GetType() == json::NULLVal)
	//Error! not a valid JSON string.

As we've explained earlier, a JSON data structure is either an array or an object. So
you have a few options for working with the "my_data" variable we defined above.
You'll recall from the previous sections that the Value class lets you index into
it with the [] operator if it represents an array or object (which, in the case of
a call to json::Deserialize, it certainly does if it's valid JSON). You could use those,
along with, in the case of an object type, the HasKey/HasKeys methods.
Or, if you prefer to cast it to an array or object, just simply do:

json::Object my_object = my_data.ToObject();	// Assuming my_data.GetType() == json::ObjectVal
json::Array my_array = my_data.ToArray();		// Assuming my_data.GetType() == json::ArrayVal


EXCEPTIONS & ERRORS: WHAT TO DO WHEN STUFF BREAKS
=================================================

As of 7/21/2014 asserts are gone and instead, we have exceptions when things go bad.
METHODS THAT THROW EXCEPTIONS ARE CLEARLY MARKED IN JSON.H. ANY METHOD THAT DOES NOT
STATE THAT IT THROWS AN EXCEPTION WILL NOT.

Let's go through an example of checking for exceptions and dealing with them.

json::Value int_value(12345);

try
{
	// This will cause an error. int_value isn't a string so calling .ToString() is not valid.
	std::string str = int_value.ToString();
}
catch (const std::runtime_error& e)
{
	// handle the error. You can check the error message by calling e.what()
	printf("Error encountered: %s\n", e.what());
}

try
{
	// also causes an error!
	int i = int_value[4];
}
catch (const std::runtime_error& e)
{
	// we tried to index into "int_value", which isn't an array type so that's not valid and we 
	// get an exception.
}


BUT I DON'T LIKE EXCEPTIONS! Well that's fine, don't use them. If you want to avoid crashes and errors, make
sure you use the .GetType() method in the Value class. Let's rewrite the above without exceptions:

json::Value int_value(12345);

if (int_value.GetType() == json::StringVal)
{
	// Obviously this won't ever execute because int_value is of type json::IntVal
	std::string str = int_value.ToString();
}

if (int_value.GetType() == json::ArrayVal)
{
	// Obviously this won't ever execute because int_value is of type json::IntVal, not json::ArrayVal
	int i = int_value[4];
}

CHANGELOG:
==========
8/31/2014:
----------
* Fixed bug from last update that broke false/true boolean usage. Courtesy of Vasi B.
* Change postfix increment of iterators in Serialize to prefix, courtesy of Vasi B.
* More improvements to validity checking of non string/object/array types. Should
	catch even more invalid usage types such as -1jE5, falsee, trueeeeeee
	{"key" : potato} (that should be {"key" : "potato"}), etc. 
* Switched to strtol and strtod from atof/atoi in Serialize for better error handling.
* Fix for GCC order of initialization warnings, courtsey of Vasi B.

8/17/2014:
----------
* Better error handling (and bug fixing) for invalid JSON. Previously, something such as:
	{"def": j[{"a": 100}],"abc": 123}
	would result in at best, a crash, and at worst, nothing when this was passed to 
	the Deserialize method. Note that the "j" is invalid in this example. This led
	to further fixes for other invalid syntax:
	- Use of multiple 'e', for example: 1ee4 is not valid 
	- Use of '.' when not preceded by a digit is invalid. For example: .1 is
		incorrect, but 0.1 is fine.
	- Using 'e' when not preceded by a digit. For example, e4 isn't valid but 1e4 is.

	The deserialize method should properly handle these problems and when there's an
	error, it returns a Value object with the NULLVal type. Check this type to see
	if there's an error.

	Issue reported by Imre Pechan.

7/21/2014:
----------
* All asserts removed and replaced with exceptions, as per request from many users.
	Instead of asserting, functions will throw a std::runtime_error with
	appropriate error message.
* Added versions of the Value::To* functions that take a default parameter.
	In the event of an error (like calling Value::ToInt() when it's type is an Object),
	the default value you specified will be returned. Courtesy of PeterSvP 
* Fixed type mismatch warning, courtesy of Per Rovegård
* Initialized some variables in the various Value constructors to defaults for
	better support with full blast g++ warnings, courtesy of Mark Odell.
* Changed Value::ToString to return a const std::string& instead of std::string
	to avoid unnecessary copying.
* Improved some commenting
* Fixed a bug where a capital E for scientific notation numbers wasn't
	recognized, only lowercase e.
* VASTLY OVERHAULED AND IMPROVED THE README FILE, PLEASE CONSULT IT FOR
	IN DEPTH USAGE AND EXAMPLES.

2/8/2014:
--------- 
MAJOR BUG FIXES, all courtesy of Per Rovegård, Ph.D.
* Feature request: HasKey and HasKeys added to Value for convenience and
	to avoid having to make a temporary object.
* Strings should now be properly unescaped. Previously, as an example, the
	string "\/Date(1390431949211+0100)\/\" would be parsed as
	\/Date(1390431949211+0100)\/. The string is now properly parsed as
	/Date(1390431949211+0100)/.
	As per http://www.json.org the other escape characters including
	\u+4 hex digits will now be properly unescaped. So for example,
	\u0061 now becomes "A".
* Serialize now supports serializing a toplevel array (which is valid JSON).
	The parameter it takes is now a Value, but existing code doesn't
	need to be changed.
* Fixed bug with checking for proper opening/closing sequence for braces/brackets.
	Previously, this code: 
		const char *json = "{\"arr\":[{}}]}";
		auto val = json::Deserialize(json);
	worked fine with no errors. That's a bug. I did a major overhaul so that
	now improperly formatted pairs will now correctly result in an error.
* Made internal deserialize methods static

1/30/2014:
----------
* Changed #pragma once to the standard #ifndef header guard style for
	better compatibility.
* Added a [] operator for Value that takes a const char* as an argument
	to avoid having to explicitly (and annoyingly) cast to std::string.
	Thus, my_value["asdf"] = "a string" should now work fine.
	The same has been added to the Object class.
* Added non-operator methods of casting a Value to int/string/bool/etc.
	Implicitly casting a Value to a std::string doesn't work as per C++
	rules. As such, previously to assign a Value to a std::string you
	had to do:
		my_std_string = (std::string)my_value;
	You can now instead do:
		my_std_string = my_value.ToString();
	If you want more information on why this can't be done, please read
	this topic for more details:
	http://stackoverflow.com/questions/3518145/c-overloading-conversion-operator-for-custom-type-to-stdstring
		
1/27/2014
----------
* Deserialize will now return a NULLType Value instance if there was an
	error instead of asserting. This way you can handle however you want to
	invalid JSON being passed in. As a top level object must be either an
	array or an object, a NULL value return indicates an invalid result.
		
1/11/2014
---------
* Major bug fix: Strings containing []{} characters could cause
	parsing errors under certain conditions. I've just tested
	the class parsing a 300KB JSON file with all manner of bizarre
	characters and permutations and it worked, so hopefully this should
	be the end of "major bug" fixes.
		
1/10/2014
---------
Bug fixes courtesy of Gerry Beauregard:
* Pretty big bug: was using wrong string paramter in ::Deserialize
	and furthermore it wasn't being trimmed. 
* Object::HasKeys now casts the return value to avoid compiler warnings.
* Slight optimization to the Trim function
* Made asserts in ::Deserialize easier to read
 	
1/9/2014
--------
* Major bug fix: for JSON strings containing \" (as in, two characters,
	not the escaped " character), the lib would mess up and not parse
	correctly.
* Major bug fix: I erroneously was assuming that all root JSON types
	had to be an object. This was an oversight, as a root JSON
	object can be an array. I have therefore changed the Deserialize
	method to return a json::Value rather than a json::Object. This
	will NOT impact any existing code you have, as a json::Value will
	cast to a json::Object (if it is indeed an object). But for 
	correctness, you should be using json::Value = Deserialize...
	The Value type can be checked if it's an array (or any other type),
	and furthermore can even be accessed with the [] operator for
	convenience.
* I've made the NULL value type set numeric fields to 0 and bool to false.
	This is for convenience for using the NULL type as a default return
	value in your code.
* asserts added to casting (Gerry Beauregard)
* Added method HasKeys to json::Object which will check if all the keys
	specified are in the object, returning the index of the first key
	not found or -1 if all found (hoppe).

1/4/2014
--------
* Fixed bug where booleans were being parsed as doubles (Gerry Beauregard).

1/2/2014 v3
------------
* More missing headers added for VisualStudio 2012
* Switched to snprintf instead of sprintf (or sprintf_s in MSVC)

1/2/2014 v2
-----------
* Added yet more missing headers for compiling on GNU and Linux systems
* Made Deserialize copy the passed in string so it won't mangle it

1/2/2014
--------
* Fixed previous changelog years. Got ahead of myself and marked them
	as 2014 when they were in fact done in 2013.
* Added const version of [] to Array/Object/Value
* Removed C++11 requirements, should work with older compilers
	(thanks to Meng Wang for pointing that out)
* Made ValueMap and ValueVector typedefs in Object/Value public
	so you can actually iterate over the class
* Added HasKey and HasValue to Object/Array for convenience
	(note this could have been done comparing .find to .end)

12/29/2013 v2
-------------
* Added .size() field to Value. Returns 1 for non Array/Object types,
	otherwise the number of elements contained.
* Added .find() to Object to search for a key. Returns Object::end()
	if not found, otherwise the Value.
	Example: bool found = my_obj.find("some key") != my_obj.end();
* Added .find() to Array to search for a value. Just a convenience
	wrapper for std::find(Array::begin(), Array::end(), Value)
* Added ==, !=, <, >, <=, >= operators to Object/Array/Value.
	For Objects/Arrays, the operators function just like they do for a
	std::map and std::vector, respectively.
* Added IsNumeric to Value to indicate if it's an int/float/double type.

12/29/2013
----------
* Added the DoubleVal type which stores, you guessed it, double values.
* Bug fix for floats with an exact integer value. Now, setting any numerical
	field will also set the fields for the other numerical types. So if you
	have obj["value"] = 12, then the int/float/double cast methods will
	return 12/12.0f/12.0. Previously, in the example above, only the int
	value was set, making a cast to float return 0.
* Bug fix for deserializing JSON strings that contained large integer values.
	Now if the numerical value of a key in a JSON string contains a number
	less than INT_MIN or greater than INT_MAX it will be stored as a double.
	Note that as mentioned above, all numerical fields are set.
* Should work fine with scientific notation values now.

12/28/2013
----------

* Fixed a bug where if there were spaces around values or key names in a JSON
string passed in to Deserialize, invalid results or asserts would occur.
(Fix courtesy of Gerry Beauregard)

* Added method named "Clear()" to Object/Array/Value to reset state

* Added license to header file for easyness (totally valid word).