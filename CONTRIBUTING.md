# Style Guide:

### Header/Source Split & Namespaces:
Header files should go in `include/` and the corresponding source files should go in `src/`. Paths to a header/source pair should be identical except for `include/` or `src/`, so as an example: 
> include/dir1/file.hpp \
> src/dir1/file.cpp

Use forward declaration wherever possible, *especially* for classes and structs to avoid polluting header namespaces. If possible, include those definitions in the source file instead. All code specific to this project should be inside the `dfd` namespace to further protect the global namespace. 

***DO NOT USE `using namespace` EVER***. Avoid `using` altogether in header files *unless* defining a custom type.

---

### Tabbing:
Spaces only, so there's no ambiguity about code alignment. There should be an option somewhere in your editor to have tab characters replaced automatically with spaces.

X spaces for tabs.

---

### Function Declaration:
camelCase, so: `addTwoNumbers();`

---

### Classes and Structs:
PascalCase, so: `ClientInfo {};`

---

### Vars:
snake_case, so `int time_seconds;`

---

### Macros:
SCREAMING_SNAKE_CASE, so `#define BUFF_SIZE = 4096;`

---

### Pointer and References:
Emphasis on type rather than var name, so `int* a;` not `int *a;`

---

### Misc:
Opening function braces should be on the same line as the function declaration, so `void addTwoNumbers(...) {`

If function declarations are too long for a single line (80+ chars) they should be split, leaving the first argument on the same line as the function declaration. Ideally align brackets, types, and names with each other respectively.
```
int addNineNumbers(int& a,
                   int& b,
                   int& c,
                   int& d,
                   int& e,
                   int& f,
                   int& g,
                   int& h
                  ) { 
    ...
}
```

Don't mix if and else clauses if one uses `{}` while they other doesn't, so:
```
if () {
    ...
} else {
    ...
} //okay

if ()
    ...
else
    ...
//okay

if () 
    ...
else {
    ...
} //not okay
```
