# Style Guide:

### Header/Source Split & Namespaces:
Header files should go in `include/` and the corresponding source files should go in `src/`. Paths to a header/source pair should be identical except for `include/` or `src/`, so as an example: 
> include/dir1/file.hpp \
> src/dir1/file.cpp

Use forward declaration wherever possible, *especially* for classes and structs to avoid polluting header namespaces. If possible, include those definitions in the source file instead. All code specific to this project should be inside the `dfd` namespace to further protect the global namespace. 

***DO NOT USE `using namespace` EVER***. Avoid `using` altogether in header files *unless* defining a custom type.

### Comment Format:
Functions: 
```
/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * funcName
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> desc, all aligned...................................................
 *    like this..............don't write past the horizontal dividers like this.
 *
 * Takes:
 * -> arg1Name:
 *    desc aligned with name. no new -> until the next arg.
 * 
 * Throws: (include if errors are possible)
 * -> errOneName:
 *    what causes error.
 *
 * Returns:
 * -> On success:
 *    desc, aligned like above.
 * -> On failure:
 *    desc. 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */ 
```

Ideally, try not to throw errors in functions and instead utilize a return code. Multiple return types can be defined under `On failure`.

Classes and Structs:
```
/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * ClassOrStructName
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> This class does...
 *
 * Member Variables:
 * -> var_name:
 *    high level desc.
 * 
 * Don't include this line but if a class should also include:
 * Constructor:
 * -> Takes:
 *    -> arg1Name:
 *       desc aligned.
 *    -> arg2Name:
 *       desc aligned.
 * -> Throws: (include if errors are possible)
 *    -> err1Name:
 *       how error occurs, aligned as usual.
 * REPEAT FOR DESTRUCTOR AND COPY/MOVE CONSTRUCTORS IF DEFINED.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
```

Member function comments should be inline. Since constructors have no return type errors *should* be thrown to indicate something going wrong. Ideally, we design around this to make failure hard in the first place if you're able to create the object.

---

### Tabbing:
Spaces only, so there's no ambiguity about code alignment. There should be an option somewhere in your editor to have tab characters replaced automatically with spaces.

4 spaces for tabs.

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
