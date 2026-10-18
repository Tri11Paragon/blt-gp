/*
 *  Traits reflection tests
 *  Copyright (C) 2026  Brett Terpstra
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <blt/gp/util/traits.h>
#include "../testing.h"
#include <iostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <functional>
#include <cstdlib>

namespace
{
    using namespace blt::gp::util;

    // Helper dummy structs and classes for testing
    struct DummyData
    {
        int id{0};
        std::string name;
    };

    struct TestClass
    {
        // Non-const member functions
        void member_void_0() {}
        int member_int_2(double, const std::string&) { return 42; }

        // Const member functions
        void member_const_0() const {}
        double member_const_1(int) const { return 3.14; }

        // Volatile member functions
        void member_volatile_0() volatile {}
        int member_volatile_1(char) volatile { return 1; }

        // Const volatile member functions
        void member_cv_0() const volatile {}
        float member_cv_2(int, double) const volatile { return 0.5f; }

        // Noexcept member functions
        void member_noexcept_0() noexcept {}
        int member_noexcept_2(double, int) noexcept { return 10; }

        // Const noexcept member functions
        void member_const_noexcept_0() const noexcept {}
        std::string member_const_noexcept_1(int) const noexcept { return "test"; }

        // Volatile noexcept member functions
        void member_volatile_noexcept_0() volatile noexcept {}
        int member_volatile_noexcept_1(char) volatile noexcept { return 1; }

        // Const volatile noexcept member functions
        void member_cv_noexcept_0() const volatile noexcept {}
        float member_cv_noexcept_2(int, double) const volatile noexcept { return 0.5f; }
    };

    // Custom functor structs
    struct CustomFunctorConst
    {
        int operator()(double, float) const { return 100; }
    };

    struct CustomFunctorNonConst
    {
        void operator()(int, const std::string&) {}
    };

    struct CustomFunctorNoexcept
    {
        double operator()(int) const noexcept { return 2.718; }
    };

    struct CustomFunctorMutableNoexcept
    {
        int count{0};
        bool operator()(int x) noexcept
        {
            count += x;
            return count > 0;
        }
    };

    // Free functions for pointer and reference testing
    [[maybe_unused]] void free_void_0() {}
    [[maybe_unused]] int free_int_2(double, const std::string&) { return 0; }
    [[maybe_unused]] void free_void_0_noexcept() noexcept {}
    [[maybe_unused]] int free_int_3_noexcept(int, float, char) noexcept { return 1; }
    [[maybe_unused]] int& free_ref_return(int& x) { return x; }

    void test_plain_functions()
    {
        std::cout << "Testing plain function types..." << std::endl;

        // 1. void()
        using T1 = void();
        using Traits1 = function_traits<T1>;
        static_assert(std::is_same_v<Traits1::return_type, void>);
        static_assert(std::is_same_v<Traits1::args_tuple, std::tuple<>>);
        static_assert(Traits1::arity == 0);
        static_assert(!Traits1::is_noexcept);
        TEST_ASSERT(Traits1::arity == 0);
        TEST_ASSERT(!Traits1::is_noexcept);

        // 2. int(double, const std::string&)
        using T2 = int(double, const std::string&);
        using Traits2 = function_traits<T2>;
        static_assert(std::is_same_v<Traits2::return_type, int>);
        static_assert(std::is_same_v<Traits2::args_tuple, std::tuple<double, const std::string&>>);
        static_assert(Traits2::arity == 2);
        static_assert(!Traits2::is_noexcept);
        static_assert(std::is_same_v<Traits2::arg<0>, double>);
        static_assert(std::is_same_v<Traits2::arg<1>, const std::string&>);
        TEST_ASSERT(Traits2::arity == 2);
        TEST_ASSERT(!Traits2::is_noexcept);

        // 3. void() noexcept
        using T3 = void() noexcept;
        using Traits3 = function_traits<T3>;
        static_assert(std::is_same_v<Traits3::return_type, void>);
        static_assert(std::is_same_v<Traits3::args_tuple, std::tuple<>>);
        static_assert(Traits3::arity == 0);
        static_assert(Traits3::is_noexcept);
        TEST_ASSERT(Traits3::arity == 0);
        TEST_ASSERT(Traits3::is_noexcept);

        // 4. int(int, float, char) noexcept
        using T4 = int(int, float, char) noexcept;
        using Traits4 = function_traits<T4>;
        static_assert(std::is_same_v<Traits4::return_type, int>);
        static_assert(std::is_same_v<Traits4::args_tuple, std::tuple<int, float, char>>);
        static_assert(Traits4::arity == 3);
        static_assert(Traits4::is_noexcept);
        static_assert(std::is_same_v<Traits4::arg<0>, int>);
        static_assert(std::is_same_v<Traits4::arg<1>, float>);
        static_assert(std::is_same_v<Traits4::arg<2>, char>);
        TEST_ASSERT(Traits4::arity == 3);
        TEST_ASSERT(Traits4::is_noexcept);

        // 5. Reference return, pointer, and rvalue arguments
        using T5 = int&(int&, const DummyData*, DummyData&&);
        using Traits5 = function_traits<T5>;
        static_assert(std::is_same_v<Traits5::return_type, int&>);
        static_assert(std::is_same_v<Traits5::args_tuple, std::tuple<int&, const DummyData*, DummyData&&>>);
        static_assert(Traits5::arity == 3);
        static_assert(!Traits5::is_noexcept);
        static_assert(std::is_same_v<Traits5::arg<0>, int&>);
        static_assert(std::is_same_v<Traits5::arg<1>, const DummyData*>);
        static_assert(std::is_same_v<Traits5::arg<2>, DummyData&&>);
        TEST_ASSERT(Traits5::arity == 3);
        TEST_ASSERT(!Traits5::is_noexcept);
    }

    void test_function_pointers()
    {
        std::cout << "Testing function pointers..." << std::endl;

        // 1. void (*)()
        using T1 = void (*)();
        using Traits1 = function_traits<T1>;
        static_assert(std::is_same_v<Traits1::return_type, void>);
        static_assert(std::is_same_v<Traits1::args_tuple, std::tuple<>>);
        static_assert(Traits1::arity == 0);
        static_assert(!Traits1::is_noexcept);
        TEST_ASSERT(Traits1::arity == 0);
        TEST_ASSERT(!Traits1::is_noexcept);

        // 2. decltype(&free_int_2) -> int (*)(double, const std::string&)
        using T2 = decltype(&free_int_2);
        using Traits2 = function_traits<T2>;
        static_assert(std::is_same_v<Traits2::return_type, int>);
        static_assert(std::is_same_v<Traits2::args_tuple, std::tuple<double, const std::string&>>);
        static_assert(Traits2::arity == 2);
        static_assert(!Traits2::is_noexcept);
        static_assert(std::is_same_v<Traits2::arg<0>, double>);
        static_assert(std::is_same_v<Traits2::arg<1>, const std::string&>);
        TEST_ASSERT(Traits2::arity == 2);
        TEST_ASSERT(!Traits2::is_noexcept);

        // 3. void (*)() noexcept
        using T3 = decltype(&free_void_0_noexcept);
        using Traits3 = function_traits<T3>;
        static_assert(std::is_same_v<Traits3::return_type, void>);
        static_assert(std::is_same_v<Traits3::args_tuple, std::tuple<>>);
        static_assert(Traits3::arity == 0);
        static_assert(Traits3::is_noexcept);
        TEST_ASSERT(Traits3::arity == 0);
        TEST_ASSERT(Traits3::is_noexcept);

        // 4. int (*)(int, float, char) noexcept
        using T4 = decltype(&free_int_3_noexcept);
        using Traits4 = function_traits<T4>;
        static_assert(std::is_same_v<Traits4::return_type, int>);
        static_assert(std::is_same_v<Traits4::args_tuple, std::tuple<int, float, char>>);
        static_assert(Traits4::arity == 3);
        static_assert(Traits4::is_noexcept);
        static_assert(std::is_same_v<Traits4::arg<0>, int>);
        static_assert(std::is_same_v<Traits4::arg<1>, float>);
        static_assert(std::is_same_v<Traits4::arg<2>, char>);
        TEST_ASSERT(Traits4::arity == 3);
        TEST_ASSERT(Traits4::is_noexcept);

        // 5. Reference return: decltype(&free_ref_return)
        using T5 = decltype(&free_ref_return);
        using Traits5 = function_traits<T5>;
        static_assert(std::is_same_v<Traits5::return_type, int&>);
        static_assert(std::is_same_v<Traits5::args_tuple, std::tuple<int&>>);
        static_assert(Traits5::arity == 1);
        static_assert(!Traits5::is_noexcept);
        static_assert(std::is_same_v<Traits5::arg<0>, int&>);
        TEST_ASSERT(Traits5::arity == 1);
        TEST_ASSERT(!Traits5::is_noexcept);
    }

    void test_function_references()
    {
        std::cout << "Testing function references..." << std::endl;

        // 1. void (&)()
        using T1 = void (&)();
        using Traits1 = function_traits<T1>;
        static_assert(std::is_same_v<Traits1::return_type, void>);
        static_assert(std::is_same_v<Traits1::args_tuple, std::tuple<>>);
        static_assert(Traits1::arity == 0);
        static_assert(!Traits1::is_noexcept);
        TEST_ASSERT(Traits1::arity == 0);
        TEST_ASSERT(!Traits1::is_noexcept);

        // 2. decltype((free_int_2)) -> int (&)(double, const std::string&)
        using T2 = decltype((free_int_2));
        using Traits2 = function_traits<T2>;
        static_assert(std::is_same_v<Traits2::return_type, int>);
        static_assert(std::is_same_v<Traits2::args_tuple, std::tuple<double, const std::string&>>);
        static_assert(Traits2::arity == 2);
        static_assert(!Traits2::is_noexcept);
        static_assert(std::is_same_v<Traits2::arg<0>, double>);
        static_assert(std::is_same_v<Traits2::arg<1>, const std::string&>);
        TEST_ASSERT(Traits2::arity == 2);
        TEST_ASSERT(!Traits2::is_noexcept);

        // 3. void (&)() noexcept
        using T3 = decltype((free_void_0_noexcept));
        using Traits3 = function_traits<T3>;
        static_assert(std::is_same_v<Traits3::return_type, void>);
        static_assert(std::is_same_v<Traits3::args_tuple, std::tuple<>>);
        static_assert(Traits3::arity == 0);
        static_assert(Traits3::is_noexcept);
        TEST_ASSERT(Traits3::arity == 0);
        TEST_ASSERT(Traits3::is_noexcept);

        // 4. int (&)(int, float, char) noexcept
        using T4 = decltype((free_int_3_noexcept));
        using Traits4 = function_traits<T4>;
        static_assert(std::is_same_v<Traits4::return_type, int>);
        static_assert(std::is_same_v<Traits4::args_tuple, std::tuple<int, float, char>>);
        static_assert(Traits4::arity == 3);
        static_assert(Traits4::is_noexcept);
        static_assert(std::is_same_v<Traits4::arg<0>, int>);
        static_assert(std::is_same_v<Traits4::arg<1>, float>);
        static_assert(std::is_same_v<Traits4::arg<2>, char>);
        TEST_ASSERT(Traits4::arity == 3);
        TEST_ASSERT(Traits4::is_noexcept);
    }

    void test_member_function_pointers()
    {
        std::cout << "Testing member function pointers..." << std::endl;

        // 1. Non-const: void (TestClass::*)()
        using T1 = decltype(&TestClass::member_void_0);
        using Traits1 = function_traits<T1>;
        static_assert(std::is_same_v<Traits1::class_type, TestClass>);
        static_assert(std::is_same_v<Traits1::return_type, void>);
        static_assert(std::is_same_v<Traits1::args_tuple, std::tuple<>>);
        static_assert(Traits1::arity == 0);
        static_assert(!Traits1::is_noexcept);
        TEST_ASSERT(Traits1::arity == 0);
        TEST_ASSERT(!Traits1::is_noexcept);

        // 2. Non-const with args: int (TestClass::*)(double, const std::string&)
        using T2 = decltype(&TestClass::member_int_2);
        using Traits2 = function_traits<T2>;
        static_assert(std::is_same_v<Traits2::class_type, TestClass>);
        static_assert(std::is_same_v<Traits2::return_type, int>);
        static_assert(std::is_same_v<Traits2::args_tuple, std::tuple<double, const std::string&>>);
        static_assert(Traits2::arity == 2);
        static_assert(!Traits2::is_noexcept);
        static_assert(std::is_same_v<Traits2::arg<0>, double>);
        static_assert(std::is_same_v<Traits2::arg<1>, const std::string&>);
        TEST_ASSERT(Traits2::arity == 2);
        TEST_ASSERT(!Traits2::is_noexcept);

        // 3. Const: void (TestClass::*)() const
        using T3 = decltype(&TestClass::member_const_0);
        using Traits3 = function_traits<T3>;
        static_assert(std::is_same_v<Traits3::class_type, TestClass>);
        static_assert(std::is_same_v<Traits3::return_type, void>);
        static_assert(std::is_same_v<Traits3::args_tuple, std::tuple<>>);
        static_assert(Traits3::arity == 0);
        static_assert(!Traits3::is_noexcept);
        TEST_ASSERT(Traits3::arity == 0);
        TEST_ASSERT(!Traits3::is_noexcept);

        // 4. Const with args: double (TestClass::*)(int) const
        using T4 = decltype(&TestClass::member_const_1);
        using Traits4 = function_traits<T4>;
        static_assert(std::is_same_v<Traits4::class_type, TestClass>);
        static_assert(std::is_same_v<Traits4::return_type, double>);
        static_assert(std::is_same_v<Traits4::args_tuple, std::tuple<int>>);
        static_assert(Traits4::arity == 1);
        static_assert(!Traits4::is_noexcept);
        static_assert(std::is_same_v<Traits4::arg<0>, int>);
        TEST_ASSERT(Traits4::arity == 1);
        TEST_ASSERT(!Traits4::is_noexcept);

        // 5. Volatile: void (TestClass::*)() volatile
        using T5 = decltype(&TestClass::member_volatile_0);
        using Traits5 = function_traits<T5>;
        static_assert(std::is_same_v<Traits5::class_type, TestClass>);
        static_assert(std::is_same_v<Traits5::return_type, void>);
        static_assert(std::is_same_v<Traits5::args_tuple, std::tuple<>>);
        static_assert(Traits5::arity == 0);
        static_assert(!Traits5::is_noexcept);
        TEST_ASSERT(Traits5::arity == 0);
        TEST_ASSERT(!Traits5::is_noexcept);

        // 6. Volatile with args: int (TestClass::*)(char) volatile
        using T6 = decltype(&TestClass::member_volatile_1);
        using Traits6 = function_traits<T6>;
        static_assert(std::is_same_v<Traits6::class_type, TestClass>);
        static_assert(std::is_same_v<Traits6::return_type, int>);
        static_assert(std::is_same_v<Traits6::args_tuple, std::tuple<char>>);
        static_assert(Traits6::arity == 1);
        static_assert(!Traits6::is_noexcept);
        static_assert(std::is_same_v<Traits6::arg<0>, char>);
        TEST_ASSERT(Traits6::arity == 1);
        TEST_ASSERT(!Traits6::is_noexcept);

        // 7. Const volatile: void (TestClass::*)() const volatile
        using T7 = decltype(&TestClass::member_cv_0);
        using Traits7 = function_traits<T7>;
        static_assert(std::is_same_v<Traits7::class_type, TestClass>);
        static_assert(std::is_same_v<Traits7::return_type, void>);
        static_assert(std::is_same_v<Traits7::args_tuple, std::tuple<>>);
        static_assert(Traits7::arity == 0);
        static_assert(!Traits7::is_noexcept);
        TEST_ASSERT(Traits7::arity == 0);
        TEST_ASSERT(!Traits7::is_noexcept);

        // 8. Const volatile with args: float (TestClass::*)(int, double) const volatile
        using T8 = decltype(&TestClass::member_cv_2);
        using Traits8 = function_traits<T8>;
        static_assert(std::is_same_v<Traits8::class_type, TestClass>);
        static_assert(std::is_same_v<Traits8::return_type, float>);
        static_assert(std::is_same_v<Traits8::args_tuple, std::tuple<int, double>>);
        static_assert(Traits8::arity == 2);
        static_assert(!Traits8::is_noexcept);
        static_assert(std::is_same_v<Traits8::arg<0>, int>);
        static_assert(std::is_same_v<Traits8::arg<1>, double>);
        TEST_ASSERT(Traits8::arity == 2);
        TEST_ASSERT(!Traits8::is_noexcept);

        // 9. Noexcept non-const: void (TestClass::*)() noexcept
        using T9 = decltype(&TestClass::member_noexcept_0);
        using Traits9 = function_traits<T9>;
        static_assert(std::is_same_v<Traits9::class_type, TestClass>);
        static_assert(std::is_same_v<Traits9::return_type, void>);
        static_assert(std::is_same_v<Traits9::args_tuple, std::tuple<>>);
        static_assert(Traits9::arity == 0);
        static_assert(Traits9::is_noexcept);
        TEST_ASSERT(Traits9::arity == 0);
        TEST_ASSERT(Traits9::is_noexcept);

        // 10. Noexcept non-const with args: int (TestClass::*)(double, int) noexcept
        using T10 = decltype(&TestClass::member_noexcept_2);
        using Traits10 = function_traits<T10>;
        static_assert(std::is_same_v<Traits10::class_type, TestClass>);
        static_assert(std::is_same_v<Traits10::return_type, int>);
        static_assert(std::is_same_v<Traits10::args_tuple, std::tuple<double, int>>);
        static_assert(Traits10::arity == 2);
        static_assert(Traits10::is_noexcept);
        static_assert(std::is_same_v<Traits10::arg<0>, double>);
        static_assert(std::is_same_v<Traits10::arg<1>, int>);
        TEST_ASSERT(Traits10::arity == 2);
        TEST_ASSERT(Traits10::is_noexcept);

        // 11. Noexcept const: void (TestClass::*)() const noexcept
        using T11 = decltype(&TestClass::member_const_noexcept_0);
        using Traits11 = function_traits<T11>;
        static_assert(std::is_same_v<Traits11::class_type, TestClass>);
        static_assert(std::is_same_v<Traits11::return_type, void>);
        static_assert(std::is_same_v<Traits11::args_tuple, std::tuple<>>);
        static_assert(Traits11::arity == 0);
        static_assert(Traits11::is_noexcept);
        TEST_ASSERT(Traits11::arity == 0);
        TEST_ASSERT(Traits11::is_noexcept);

        // 12. Noexcept const with args: std::string (TestClass::*)(int) const noexcept
        using T12 = decltype(&TestClass::member_const_noexcept_1);
        using Traits12 = function_traits<T12>;
        static_assert(std::is_same_v<Traits12::class_type, TestClass>);
        static_assert(std::is_same_v<Traits12::return_type, std::string>);
        static_assert(std::is_same_v<Traits12::args_tuple, std::tuple<int>>);
        static_assert(Traits12::arity == 1);
        static_assert(Traits12::is_noexcept);
        static_assert(std::is_same_v<Traits12::arg<0>, int>);
        TEST_ASSERT(Traits12::arity == 1);
        TEST_ASSERT(Traits12::is_noexcept);

        // 13. Volatile noexcept: void (TestClass::*)() volatile noexcept
        using T13 = decltype(&TestClass::member_volatile_noexcept_0);
        using Traits13 = function_traits<T13>;
        static_assert(std::is_same_v<Traits13::class_type, TestClass>);
        static_assert(std::is_same_v<Traits13::return_type, void>);
        static_assert(std::is_same_v<Traits13::args_tuple, std::tuple<>>);
        static_assert(Traits13::arity == 0);
        static_assert(Traits13::is_noexcept);
        TEST_ASSERT(Traits13::arity == 0);
        TEST_ASSERT(Traits13::is_noexcept);

        // 14. Volatile noexcept with args: int (TestClass::*)(char) volatile noexcept
        using T14 = decltype(&TestClass::member_volatile_noexcept_1);
        using Traits14 = function_traits<T14>;
        static_assert(std::is_same_v<Traits14::class_type, TestClass>);
        static_assert(std::is_same_v<Traits14::return_type, int>);
        static_assert(std::is_same_v<Traits14::args_tuple, std::tuple<char>>);
        static_assert(Traits14::arity == 1);
        static_assert(Traits14::is_noexcept);
        static_assert(std::is_same_v<Traits14::arg<0>, char>);
        TEST_ASSERT(Traits14::arity == 1);
        TEST_ASSERT(Traits14::is_noexcept);

        // 15. Const volatile noexcept: void (TestClass::*)() const volatile noexcept
        using T15 = decltype(&TestClass::member_cv_noexcept_0);
        using Traits15 = function_traits<T15>;
        static_assert(std::is_same_v<Traits15::class_type, TestClass>);
        static_assert(std::is_same_v<Traits15::return_type, void>);
        static_assert(std::is_same_v<Traits15::args_tuple, std::tuple<>>);
        static_assert(Traits15::arity == 0);
        static_assert(Traits15::is_noexcept);
        TEST_ASSERT(Traits15::arity == 0);
        TEST_ASSERT(Traits15::is_noexcept);

        // 16. Const volatile noexcept with args: float (TestClass::*)(int, double) const volatile noexcept
        using T16 = decltype(&TestClass::member_cv_noexcept_2);
        using Traits16 = function_traits<T16>;
        static_assert(std::is_same_v<Traits16::class_type, TestClass>);
        static_assert(std::is_same_v<Traits16::return_type, float>);
        static_assert(std::is_same_v<Traits16::args_tuple, std::tuple<int, double>>);
        static_assert(Traits16::arity == 2);
        static_assert(Traits16::is_noexcept);
        static_assert(std::is_same_v<Traits16::arg<0>, int>);
        static_assert(std::is_same_v<Traits16::arg<1>, double>);
        TEST_ASSERT(Traits16::arity == 2);
        TEST_ASSERT(Traits16::is_noexcept);
    }

    void test_functors_and_lambdas()
    {
        std::cout << "Testing functors and lambdas..." << std::endl;

        // 1. Stateless const lambda
        auto lambda1 = [](int a, double b) -> float { return static_cast<float>(a + b); };
        using Traits1 = function_traits<decltype(lambda1)>;
        static_assert(std::is_same_v<Traits1::return_type, float>);
        static_assert(std::is_same_v<Traits1::args_tuple, std::tuple<int, double>>);
        static_assert(Traits1::arity == 2);
        static_assert(!Traits1::is_noexcept);
        static_assert(std::is_same_v<Traits1::arg<0>, int>);
        static_assert(std::is_same_v<Traits1::arg<1>, double>);
        TEST_ASSERT(Traits1::arity == 2);
        TEST_ASSERT(!Traits1::is_noexcept);

        // 2. Capturing lambda
        int cap = 10;
        auto lambda2 = [cap](const std::string& s) -> int { return static_cast<int>(s.size()) + cap; };
        using Traits2 = function_traits<decltype(lambda2)>;
        static_assert(std::is_same_v<Traits2::return_type, int>);
        static_assert(std::is_same_v<Traits2::args_tuple, std::tuple<const std::string&>>);
        static_assert(Traits2::arity == 1);
        static_assert(!Traits2::is_noexcept);
        static_assert(std::is_same_v<Traits2::arg<0>, const std::string&>);
        TEST_ASSERT(Traits2::arity == 1);
        TEST_ASSERT(!Traits2::is_noexcept);

        // 3. Mutable lambda (non-const operator())
        auto lambda3 = [cap](int val) mutable -> void { cap += val; };
        using Traits3 = function_traits<decltype(lambda3)>;
        static_assert(std::is_same_v<Traits3::return_type, void>);
        static_assert(std::is_same_v<Traits3::args_tuple, std::tuple<int>>);
        static_assert(Traits3::arity == 1);
        static_assert(!Traits3::is_noexcept);
        static_assert(std::is_same_v<Traits3::arg<0>, int>);
        TEST_ASSERT(Traits3::arity == 1);
        TEST_ASSERT(!Traits3::is_noexcept);

        // 4. Noexcept lambda
        auto lambda4 = [](int a, int b) noexcept -> int { return a + b; };
        using Traits4 = function_traits<decltype(lambda4)>;
        static_assert(std::is_same_v<Traits4::return_type, int>);
        static_assert(std::is_same_v<Traits4::args_tuple, std::tuple<int, int>>);
        static_assert(Traits4::arity == 2);
        static_assert(Traits4::is_noexcept);
        static_assert(std::is_same_v<Traits4::arg<0>, int>);
        static_assert(std::is_same_v<Traits4::arg<1>, int>);
        TEST_ASSERT(Traits4::arity == 2);
        TEST_ASSERT(Traits4::is_noexcept);

        // 5. Mutable noexcept lambda
        auto lambda5 = [cap](double d) mutable noexcept -> bool { return d > cap; };
        using Traits5 = function_traits<decltype(lambda5)>;
        static_assert(std::is_same_v<Traits5::return_type, bool>);
        static_assert(std::is_same_v<Traits5::args_tuple, std::tuple<double>>);
        static_assert(Traits5::arity == 1);
        static_assert(Traits5::is_noexcept);
        static_assert(std::is_same_v<Traits5::arg<0>, double>);
        TEST_ASSERT(Traits5::arity == 1);
        TEST_ASSERT(Traits5::is_noexcept);

        // 6. Custom Functor Const
        using TraitsF1 = function_traits<CustomFunctorConst>;
        static_assert(std::is_same_v<TraitsF1::return_type, int>);
        static_assert(std::is_same_v<TraitsF1::args_tuple, std::tuple<double, float>>);
        static_assert(TraitsF1::arity == 2);
        static_assert(!TraitsF1::is_noexcept);
        static_assert(std::is_same_v<TraitsF1::arg<0>, double>);
        static_assert(std::is_same_v<TraitsF1::arg<1>, float>);
        TEST_ASSERT(TraitsF1::arity == 2);
        TEST_ASSERT(!TraitsF1::is_noexcept);

        // 7. Custom Functor Non-Const
        using TraitsF2 = function_traits<CustomFunctorNonConst>;
        static_assert(std::is_same_v<TraitsF2::return_type, void>);
        static_assert(std::is_same_v<TraitsF2::args_tuple, std::tuple<int, const std::string&>>);
        static_assert(TraitsF2::arity == 2);
        static_assert(!TraitsF2::is_noexcept);
        static_assert(std::is_same_v<TraitsF2::arg<0>, int>);
        static_assert(std::is_same_v<TraitsF2::arg<1>, const std::string&>);
        TEST_ASSERT(TraitsF2::arity == 2);
        TEST_ASSERT(!TraitsF2::is_noexcept);

        // 8. Custom Functor Noexcept
        using TraitsF3 = function_traits<CustomFunctorNoexcept>;
        static_assert(std::is_same_v<TraitsF3::return_type, double>);
        static_assert(std::is_same_v<TraitsF3::args_tuple, std::tuple<int>>);
        static_assert(TraitsF3::arity == 1);
        static_assert(TraitsF3::is_noexcept);
        static_assert(std::is_same_v<TraitsF3::arg<0>, int>);
        TEST_ASSERT(TraitsF3::arity == 1);
        TEST_ASSERT(TraitsF3::is_noexcept);

        // 9. Custom Functor Mutable Noexcept
        using TraitsF4 = function_traits<CustomFunctorMutableNoexcept>;
        static_assert(std::is_same_v<TraitsF4::return_type, bool>);
        static_assert(std::is_same_v<TraitsF4::args_tuple, std::tuple<int>>);
        static_assert(TraitsF4::arity == 1);
        static_assert(TraitsF4::is_noexcept);
        static_assert(std::is_same_v<TraitsF4::arg<0>, int>);
        TEST_ASSERT(TraitsF4::arity == 1);
        TEST_ASSERT(TraitsF4::is_noexcept);
    }

    void test_std_function()
    {
        std::cout << "Testing std::function..." << std::endl;

        // 1. std::function<void()>
        using T1 = std::function<void()>;
        using Traits1 = function_traits<T1>;
        static_assert(std::is_same_v<Traits1::return_type, void>);
        static_assert(std::is_same_v<Traits1::args_tuple, std::tuple<>>);
        static_assert(Traits1::arity == 0);
        static_assert(!Traits1::is_noexcept);
        TEST_ASSERT(Traits1::arity == 0);
        TEST_ASSERT(!Traits1::is_noexcept);

        // 2. std::function<int(double, const std::string&)>
        using T2 = std::function<int(double, const std::string&)>;
        using Traits2 = function_traits<T2>;
        static_assert(std::is_same_v<Traits2::return_type, int>);
        static_assert(std::is_same_v<Traits2::args_tuple, std::tuple<double, const std::string&>>);
        static_assert(Traits2::arity == 2);
        static_assert(!Traits2::is_noexcept);
        static_assert(std::is_same_v<Traits2::arg<0>, double>);
        static_assert(std::is_same_v<Traits2::arg<1>, const std::string&>);
        TEST_ASSERT(Traits2::arity == 2);
        TEST_ASSERT(!Traits2::is_noexcept);

        // 3. std::function with 5 parameters
        using T3 = std::function<bool(int, float, double, char, void*)>;
        using Traits3 = function_traits<T3>;
        static_assert(std::is_same_v<Traits3::return_type, bool>);
        static_assert(std::is_same_v<Traits3::args_tuple, std::tuple<int, float, double, char, void*>>);
        static_assert(Traits3::arity == 5);
        static_assert(!Traits3::is_noexcept);
        static_assert(std::is_same_v<Traits3::arg<0>, int>);
        static_assert(std::is_same_v<Traits3::arg<1>, float>);
        static_assert(std::is_same_v<Traits3::arg<2>, double>);
        static_assert(std::is_same_v<Traits3::arg<3>, char>);
        static_assert(std::is_same_v<Traits3::arg<4>, void*>);
        TEST_ASSERT(Traits3::arity == 5);
        TEST_ASSERT(!Traits3::is_noexcept);
    }
}

int main()
{
    test_plain_functions();
    test_function_pointers();
    test_function_references();
    test_member_function_pointers();
    test_functors_and_lambdas();
    test_std_function();

    std::cout << "All traits tests passed successfully!" << std::endl;
    return 0;
}
