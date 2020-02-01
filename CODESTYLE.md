# Naming conventions
snake_case variable names

snake_case for implementation functions

snake_case for interface functions, except where following libc naming for whatever reason lel

# Code format
1. East const
2. Right pointers
3. Include guards (using NAME_H format)
4. No braces on newlines

## C source files
1. Includes in the following order:

Include for this source file's header

Includes with brackets

Other includes

2. Global variables (try to minimize)
3. Unexposed functions
4. Exposed functions

## C header files
1. Includes (in headers only include what you need for exposed functions and types)
2. Defines and macros
3. Enums, structs, types
4. Exposed variables
5. Exposed functions with return type of void
6. Other exposed functions (Group by return type)
7. Include argument names in function declaration
