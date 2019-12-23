# DripOS Code Styling

## General
Header and source files should always start with a multiline comment displaying the name of the file, names of the copyright holders, and then a small description of the file. The description should correspond with that in the source file.

e.x:

`math.h`
```C
/*
    math.h
    Copyright Shreyas Lad (PenetratingShot) 2019

    Useful mathematical functions

    MIT License
*/

```
`math.c`
```C
/*
    math.c
    Copyright Shreyas Lad (PenetratingShot) 2019

    Useful mathematical functions

    MIT License
*/
```

### Definitions and Variables
Variables should never be declared in header files. They should only be declared and assigned a value in source files. The only exceptions to this are when variables in the source file need to exposed for other source files, at which `extern` should be used in the header file.

e.x: `extern uint32_t num;`

The other exceptions to this rule are the declarations of structs and enums. All structs and enums must be declared in header files.

Like structs and enums, definitions (`#define`) must be defined in header files. This is to allow other source files to use them.

### Import Structure
Import statements should be grouped by where they're from. If you have 5 imports from `cpu/` and 5 from `drivers/`, the ones from `cpu/` should be next to each other and the ones from `drivers/` should be next to each other aswell. Do not separate imports.

1. std
- `stdint.h`, `stddef.h`, etc
2. libc
- `stdlib.h`, `stdio.h`, etc
3. drivers
4. fs
5. kernel
6. cpu

## Header Files
After the comment, the file should start with `#pragma once`. Do not use `#define` and `#ifndef`

#### File Structure
1. Copyright Comment
2. `#pragma once`
3. Imports
4. Definitions
5. Typedefs
6. Structs
7. Enums
8. Exposed Variables
    - Any `extern` variables exposed from the source file
9. Function Declarations

## Source Files
After the comment, source files should always start with an import of its corresponding header file and a line of whitespace under it.

#### File Structure
1. Copyright Comment
2. Header file import
3. Imports
4. Variable declarations
5. Functions