# blt-gp
Genetic Programming (GP) library for C++. Integrates directly into the C++ type system, a safe replacement for lilgp without performance compromises.

## Easy to use, Safe, and fast, all without compromise
Using blt-gp is very easy, import the program header, define your operators, and then call the relevant functions to set up the program to your liking. 
Concrete examples can be found in the example folder and compiled using the CMake argument `-DBUILD_EXAMPLES=ON`

Operator example:
```c++
// constructor takes an optional string that describes the function of the operator. Used in printing.
blt::gp::operation_t add([](float a, float b) {
  return a + b;
}, "add");
```
Operators can return different types or have differing types as arguments, this will automatically be recognized by the library.
```c++
// Any callable type can be passed as the function parameter
// so long as the function exists for the lifetime of the gp_program object.
bool compare_impl(bool type, float a, float b)
{
  return type ? (a > b) : (a < b);
}


blt::gp::operation_t compare(compare_impl, "compare");
```
Please note that if a type doesn't have a way to produce a terminal, or doesn't have a way of converting from a type that has a terminal, you will get an error.

Defining your fitness function is just as easy:
```c++
const auto fitness_function = [](blt::gp::tree_t& current_tree, blt::gp::fitness_t& fitness, blt::size_t current_index) {
    // evaluate your fitness
    // ...
    // write to the fitness out parameter. Only "adjusted_fitness" is used during evaluation.
    fitness.raw_fitness = static_cast<double>(fitness.hits);
    fitness.standardized_fitness = fitness.raw_fitness;
    // higher values = better. Should be bounded [0, 1]
    fitness.adjusted_fitness = 1.0 - (1.0 / (1.0 + fitness.standardized_fitness));
    // returning true from this function ends the evaluation of the program, as this signals that a valid solution was found.
    // this function is allowed to return void.
    return static_cast<blt::size_t>(fitness.hits) == training_cases.size();
};
```
