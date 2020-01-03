# Naming conventions
snake_case variable names
snake_case for implementation functions
snake_case for interface functions, except where following libc

# Code format
1. East const
2. Right pointers
3. Include guards (using NAME_H format)

## C source files
1. Includes
2. Global variables (try to minimize)
3. Exposed functions
4. Unexposed functions

## C header files
1. Includes (in headers only include what you need for exposed functions and such)
2. Defines and macros
3. Exposed variables
4. Exposed functions with return type of void
5. Other exposed functions (Group by return type)
6. Include argument names in function declaration
