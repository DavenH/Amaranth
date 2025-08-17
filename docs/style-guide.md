# Amaranth Code Style Guide

This document summarises formatting patterns observed in the repository. When editing files, follow the conventions already present in the surrounding code.

## Indentation

- Use spaces for indentation and for alignment, e.g. constructor initializer lists:

  ```cpp
  Initializer::Initializer() :
          SingletonAccessor(nullptr, "Initializer")
      ,   instanceId              (0)
      ,   haveInitDsp             (false)
  ```
- Library and test sources use **four spaces** for indentation:

  ```cpp
      for (int i = 0; i < a.size(); i++) {
          if (std::abs(a[i] - b[i]) > tolerance) return false;
      }
  ```

## Includes

- `#pragma once` is used at the top of headers
- Standard or module headers (`<...>`) come before project headers (`"..."`), with groups separated by blank lines
- Project includes can be further grouped by subdirectory if many appear
- After includes, place any narrow `using` declarations, forward declarations, then type aliases

## Newlines & Logical Grouping

- Order at header file top:

  1. `#pragma once`
  2. System/module includes
  3. Project includes
  5. Forward declarations
  4. `using` declarations
  6. Type aliases
  7. Class/struct definitions

- Inside classes, leave **one blank line** between logical groups:

  - Nested enums/structs/classes
  - Special members (ctor/copy/assign/dtor)
  - Public API
  - State save/restore helpers
  - Sampling/processing helpers
  - Virtual interface
  - Getters/setters
  - Overrides of base-class hooks
  - Protected helpers
  - Data members
  - Between declarations and conditionals
  - Use the section-break comment:

  ```cpp
  /* ----------------------------------------------------------------------------- */
  ```

  with one blank line above and below

## Inheritance & Declaration Wrapping

- Short single bases stay on one line; multiple or long base specifiers wrap after the colon, indented to a soft alignment column:

  ```cpp
  class MeshRasterizer :
          public Updateable {
  ...
  }
  ```
- Access specifiers are flush with class indent; leave one blank line after when following block is nontrivial

## Braces and Control Blocks

- Opening braces stay on the same line as declarations; closing braces are on their own line
- Braces are used even for single statements
- Space after control keywords: `if (condition)`, `while (x)`
- Always brace single statements in conditionals:

  ```cpp
  if (dest.empty()) {
      return;
  }
  ```

## Constructor Initializer Lists

- Colon follows the constructor signature; each member appears on its own line with **leading commas** and aligned names and parentheses:

  ```cpp
  MeshRasterizer::MeshRasterizer(const String& name) :
          name                (name)
      ,   mesh                (nullptr)
      ,   path            (nullptr)
      ,   zeroIndex           (0)
      ,   paddingSize         (2)
      ,   oneIndex            (INT_MAX / 2)
      ,   noiseSeed           (-1)
      ,   overridingDim       (Vertex::Time)
  {}
  ```
- Member order in initializer lists should match declaration order in the class

## Switch Statements

- `case` labels are indented one level inside `switch`
- Assignments in cases may be column-aligned for readability:

  ```cpp
  switch (sampleRate) {
      case 192000:    inRate  = 147;      outRate = 640;      break;
      case 176400:    inRate  = 1;        outRate = 4;        break;
  }
  ```

## Templates and Types

- No space before `<` in template arguments:

  ```cpp
  std::unique_ptr<XmlElement> topelem(new XmlElement(name));
  ```

## Macros and Preprocessor

- `#ifdef`, `#else`, and `#endif` are dedented by 2 spaces from current indent:

  ```cpp
    if (numInstances.get() == 0) {
        ++numInstances;
      #ifdef USE_IPP
        ippInit();
      #endif
        Curve::calcTable();
    } else {
        ++numInstances;
    }
  ```

## Inline One-Liners vs. Out-of-Line

- Trivial accessors (straight returns, simple predicates, single assignment) may be inline with braces on the same line:

  ```cpp
  bool doesCalcDepthDimensions() const { return calcDepthDims; }
  ```
- Anything with control flow, assertions, or touching multiple members should be declared in the header and defined in the `.cpp`

## Alignment Rules

- Getter/setter runs may align return types, function names, and braces **within that run only**
- Pointer/reference symbols attach to the type, not the name:

  ```cpp
  Mesh* getMesh();
  void  setMesh(Mesh* mesh);
  float sampleAt(double angle, int& currentIndex);
  ```

## Enums, Structs, and Nested Types

- Prefer `enum class` for new code; retain legacy unscoped enums where prevalent
- Nested POD structs live before methods and may use brace-or-equal initializers or explicit default constructors

## Virtuals, Overrides, and Specifiers

- Use `virtual` only on interface declarations; use `override` for overrides
- Do not mix `virtual` and `override` on the same declaration
- Prefer `const` correctness and `noexcept` where obvious

## Function Signatures & Defaults

- Space around `=` in default args:

  ```cpp
  void applyPaths(Intercept& icpt, const MorphPosition& morph, bool noOffsetAtEnds = false);
  ```

## Forward Declarations, Aliases, and Iterators

- Forward-declare peer classes used by pointer/reference; this limits translation unit recompilation.
- Prefer `using` over `typedef` for new code, but maintain existing style for minimal churn

## Getters/Setters & Naming

- Boolean getters may use verbs (`doesCalcDepthDimensions`) or `is/has`; be consistent within a class
- Group getters/setters together, with a blank line before and after

## Data Member Ordering & Grouping

Group members by role and volatility, separated by one blank line:

1. Static tables
2. Flags (bools)
3. Small scalars (int/short)
4. Floats and numeric params
5. Enums & small value types
6. Lightweight PODs (String, Dimensions, timers)
7. Primary containers (vectors, Buffers)
8. Heavy/auxiliary structs
9. Raw pointers (document ownership)
10. Trailing macros (e.g., `JUCE_LEAK_DETECTOR`)

Mirror this order in constructor initializer lists.

## Variable Naming

- Variables should be named with at most 4 words
- Should be self-documenting

## Method Naming

- Should have at most 4 words (plus some prefix like get-/set-/is-/create-)
- Should be self-documenting
- Name should be as precise and constraining as reasonable, so for example, avoid "handleGenericEvent()" if at all possible
  - Prefer to keep necessarily generic names as private methods, expose concrete methods as public, e.g.
```cpp 
    public:
      void convertToBipolar() { convert(PolarityType.Bipolar); }
      void convertToUnipolar() { convert(PolarityType.Unipolar); }
    private: 
      void convert(PolarityType type) { ... }
``` 
- Should specify any side effects -- state changes that aren't obviously related from the method name, 
  - e.g. `serializeDataCachingAuth()`
 

## Comments

- Use the long bar `/* ----------------------------------------------------------------------------- */` for major API sections; avoid decorative ASCII elsewhere
- Never restate obvious code with an inline comment. Use inline comments when:
  1. Clarification is needed for constants or magic numbers
  2. If a variable name needs to be necessarily truncated because of space, but the meaning is not fully captured in the variable name
  3. Peculiarities that are not obvious warrant it
  4. There are particular risks or instructions relevant to a piece of code, like "Can cause re-entrant deadlock if called twice on same thread"

## Code Reuse and Complexity

- Where possible, encapsulate complexity at the "edges" (specialization classes or instances), rather than exposing this complexity in the interface
- Try to keep methods and functions under 30 lines
- Evaluate the rough 'complexity cost' when determining whether to apply DRY (don't repeat yourself) or WET (write everything twice)
  - Score metric:
    - Code Terminals (keyword, literal, operator, method invocation etc): 1 point each
    - Declarations: 2 points each
    - Scopes (methods, functions, classes):
      - Sum of terminals in scope divided by (number of uses: unique invocations/subclasses, PLUS one)
  - This should promote DRY principle where duplicating high-complexity code, while also promoting WET in some cases where the duped code is low-complexity
- Favour composition rather than inheritance (to be fair this repo has significant tech debt from overuse of inheritance)


## Optimization

Significant work has been put into the vectorized **Buffer** and **VecOps** classes.
These are implementations of low-level BLAS operations. For speed, they use Intel IPP or Apples vDSP framework under the hood. 

### Buffer< T >
Use this wrapper to do vector operations where the desire is to mutate a particular memory block statefully.
This class will not allocate memory, or take memory ownership of memory, but it can be used to mutate arrays in memory.

**Buffer< T >** is meant for expressions where the original content is not fully replaced by method arguments. 
i.e. `x = f(x, arg1, arg2...)`

Operations can be chained, for example:
```cpp
float[] x = new float[100];
Buffer<float> buff(x, 100);

// x[i] = 5 - (10 * i / (x.size() - 1) + 1)
buff.ramp().mul(10.).add(1).subCRev(5.);  
```

Style hint: prefer to chain operations where possible. 

Note: for singular values, we can use the Real class similarly:
```cpp
const float x = Real(10.).ln().inv();
```

### VecOps< T >

This wrapper is meant for expressions that totally replace the destination array's content.
i.e. `x = f(arg1, arg2...)`

Example:

```cpp
float[] x = { 1, 2, 3 };
float[] y = { -1, -2, -3 };
float[3] z;
VecOps::add(x, y, z) 
```
### ScopedAlloc< T >

This is a subclass of Buffer which additionally takes memory ownership of the data.


## Switch Blocks

Where appropriate, throw a `std::invalid_argument` if the cases should be exhaustive. 

Example:
```cpp
Vertex* TrilinearCube::Face::operator[](const int index) const {
    switch (index) {
        case 0: return v00;
        case 1: return v01;
        case 2: return v10;
        case 3: return v11;
        default: throw std::invalid_argument("TrilinearCube::Face::[]: index out of range");;
    }
}
```

## Loops

- Prefer range-based for-loops where the iterator index is not required
- If a boolean filter is nested within the loop, prefer to flatten the nesting structure by using `if(!condition) { continue; }` rather than a nested `if (condition)` block. 
 

## Miscellaneous

- Avoid `using namespace` at global scope; single-symbol `using` at file scope is acceptable
- Use `explicit` on single-argument constructors
- Prefer `= default`/`= delete` for trivial special members
- Lines are not strictly wrapped at 80 columns; prioritize readability
- Follow existing spacing around operators and parentheses