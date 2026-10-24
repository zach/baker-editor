#pragma once
// The game's vocabulary, from its reference JSON (baker.reference/1.1): the server has no
// game, so it declares each native function from its description (Context::declare), for
// check to count and type a call's arguments against, and learns the objects the game names.
#include <baker/script.hpp>

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

namespace baker_lsp {

// A function of the game's, for hover and completion.
struct FunctionInfo {
    std::string name;
    std::vector<std::string> signatures;  // one per overload: {name symbol [int] values...}
    std::string doc;
    std::string status;  // normal, stub, provisional or deprecated
    std::string tier;    // native or script
    std::string file;    // where a script function is defined; empty for a native one
    int line = 0;
};

// A type of the game's, with the methods a call on its objects may name.
struct TypeInfo {
    std::string name;
    std::vector<std::string> bases;
    std::vector<FunctionInfo> methods;  // signatures written {Type method params...}
};

// A tweak of the game's (baker.tweaks/1): a constant its code reads from the data.
// A list's element, as the reference describes it (baker.tweaks/1.5, baker.reference/1.4):
// what each element is, so a tool checks a list of the game's own type (vec2) without it.
struct ElementInfo {
    std::string type;                 // "float32", "vec2"; empty for a value that is not a list
    std::vector<std::string> values;  // Baker's type of each of its values
    std::string count;                // "exact", "at_least" or "any"; empty if unsaid
};

struct TweakInfo {
    std::string path;               // as declared: duel.buffer
    std::vector<std::string> tags;  // the path's parts, as a data path's
    std::string type;               // as errors name it: int32, float32, bool, vec2...
    std::string default_text;       // the code's default, as Baker text
    bool bound = false;             // the data supplied the value when the reference was written
    bool ranged = false;
    double min = 0, max = 0;
    double step = 0;
    std::string desc, kind;
    std::string group;                // the tweaks a misspelled key beside them is told against
    std::vector<std::string> one_of;  // the symbols it may be (1.1); empty: any
    std::vector<std::string> values;  // Baker's type of each value (1.2); empty if unsaid
    std::string count;                // how many values: "exact", "at_least", "any" (1.4)
    ElementInfo element;              // a list's element (1.5); its type empty if not a list
    size_t length = 0;                // a fixed-size list's elements (1.5); 0: any
    bool dev_only = false;
};

// The reference's count ("exact", "at_least", "any") as Baker's; Exact if unsaid.
baker::ValueCount value_count(const std::string& count);
// A value's "count", "element" and "length", as the reference writes them for a tweak or a
// parameter.
void read_list(const nlohmann::json& value, std::string& count, ElementInfo& element,
               size_t& length);

struct Reference {
    int functions = 0;  // declared
    int overloads = 0;
    // Functions with a parameter of a type Context::declare does not know, one of the game's
    // own: declared with it taking any value.
    int widened = 0;
    std::vector<std::string> failed;                  // what could not be declared, and why
    std::vector<std::string> declared_types;          // the game's types declared (declare_type)
    int declared_objects = 0;                         // named objects declared (declare_object)
    std::vector<std::string> objects;                 // the objects the game names, such as clock
    std::map<std::string, std::string> object_types;  // each object's type, as types names it
    std::vector<TypeInfo> types;
    std::vector<FunctionInfo> all;  // every function the reference lists, Baker's too
    std::vector<TweakInfo> tweaks;  // from the stacks' {reference, tweaks, shapes}
};

// The methods a call on `object` may name: its type's, then its bases', or none if the
// object or its type is unknown.
std::vector<const FunctionInfo*> methods_of(const Reference& r, const std::string& object);

// The tweak at the data path `tags`, or null.
const TweakInfo* tweak_at(const Reference& r, const std::vector<std::string>& tags);

// Reads the reference at `path` and declares in `ctx` its native functions, its types with
// their methods, and its named objects, skipping what `ctx` already has (Baker's own). A
// function keeps its status: a stub stays a stub. False with `error` if the file is missing or
// not a reference.
bool load_reference(baker::Context& ctx, const std::string& path, Reference& out,
                    std::string& error);

}  // namespace baker_lsp
