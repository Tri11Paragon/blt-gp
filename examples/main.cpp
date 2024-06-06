/*
 *  <Short Description>
 *  Copyright (C) 2024  Brett Terpstra
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
#include <iostream>
#include <blt/gp/program.h>
#include <variant>
#include <stack>
#include <deque>
#include <vector>
#include <random>

//// small scale
//enum class op
//{
//    ADD,
//    SUB,
//    MUL,
//    DIV,
//    LIT
//};
//
//std::string to_string(op o)
//{
//    switch (o)
//    {
//        case op::ADD:
//            return "ADD";
//        case op::SUB:
//            return "SUB";
//        case op::MUL:
//            return "MUL";
//        case op::DIV:
//            return "DIV";
//        case op::LIT:
//            return "LIT";
//    }
//    return "";
//}
//
//constexpr static long SEED = 41912;
//
//op generate_op()
//{
//    static std::mt19937_64 engine(SEED);
//    static std::uniform_int_distribution dist(0, static_cast<int>(op::LIT) - 1);
//    return static_cast<op>(dist(engine));
//}
//
//bool choice()
//{
//    static std::mt19937_64 engine(SEED);
//    static std::uniform_int_distribution dist(0, 1);
//    return dist(engine);
//}
//
//float random_value()
//{
//    static std::mt19937_64 engine(SEED);
//    static std::uniform_real_distribution dist(0.0f, 10.0f);
//    return dist(engine);
//}
//
//void test()
//{
//    std::vector<op> operations;
//    std::vector<float> values;
//
//    std::stack<op> tree_generator;
//    tree_generator.push(generate_op());
//
//    while (!tree_generator.empty())
//    {
//        auto opn = tree_generator.top();
//        tree_generator.pop();
//
//        operations.push_back(opn);
//        if (opn == op::LIT)
//        {
//            values.push_back(random_value());
//            continue;
//        }
//
//        // child 1
//        if (choice())
//            tree_generator.push(generate_op());
//        else
//            tree_generator.push(op::LIT);
//
//        // child 2
//        if (choice())
//            tree_generator.push(generate_op());
//        else
//            tree_generator.push(op::LIT);
//    }
//
//    for (const auto& v : operations)
//        std::cout << to_string(v) << " ";
//    std::cout << std::endl;
//
//    {
//        std::stack<blt::size_t> process;
//        for (const auto& v : operations)
//        {
//            switch (v)
//            {
//                case op::ADD:
//                case op::SUB:
//                case op::MUL:
//                case op::DIV:
//                    process.emplace(2);
//                    std::cout << "(";
//                    break;
//                case op::LIT:
//                    break;
//            }
//            std::cout << to_string(v);
//            while (!process.empty())
//            {
//                auto top = process.top();
//                process.pop();
//                if (top == 0)
//                {
//                    std::cout << ")";
//                    continue;
//                } else
//                {
//                    std::cout << " ";
//                    process.push(top - 1);
//                    break;
//                }
//            }
//        }
//        while (!process.empty())
//        {
//            auto top = process.top();
//            process.pop();
//            if (top == 0)
//            {
//                std::cout << ") ";
//                continue;
//            } else
//            {
//                std::cerr << "FUCK YOU\n";
//                break;
//            }
//        }
//        std::cout << std::endl;
//    }
//
//    for (const auto& v : values)
//        std::cout << v << " ";
//    std::cout << std::endl;
//
//    {
//        std::stack<blt::size_t> process;
//        blt::size_t index = 0;
//        for (const auto& v : operations)
//        {
//            switch (v)
//            {
//                case op::ADD:
//                case op::SUB:
//                case op::MUL:
//                case op::DIV:
//                    process.emplace(2);
//                    std::cout << "(";
//                    std::cout << to_string(v);
//                    break;
//                case op::LIT:
//                    std::cout << values[index++];
//                    break;
//            }
//
//            while (!process.empty())
//            {
//                auto top = process.top();
//                process.pop();
//                if (top == 0)
//                {
//                    std::cout << ")";
//                    continue;
//                } else
//                {
//                    std::cout << " ";
//                    process.push(top - 1);
//                    break;
//                }
//            }
//        }
//        while (!process.empty())
//        {
//            auto top = process.top();
//            process.pop();
//            if (top == 0)
//            {
//                std::cout << ") ";
//                continue;
//            } else
//            {
//                std::cerr << "FUCK YOU\n";
//                break;
//            }
//        }
//        std::cout << std::endl;
//    }
//
//    std::stack<float> process;
//    std::stack<op> operators;
//
//    for (const auto& v : operations)
//        operators.push(v);
//
//    while (!operators.empty())
//    {
//        auto oper = operators.top();
//        operators.pop();
//        if (oper == op::LIT)
//        {
//            process.push(values.back());
//            values.pop_back();
//        } else
//        {
//            auto v1 = process.top();
//            process.pop();
//            auto v2 = process.top();
//            process.pop();
//            std::cout << "processing oper " << to_string(oper) << " with values " << v1 << " " << v2 << std::endl;
//            switch (oper)
//            {
//                case op::ADD:
//                    values.push_back(v1 + v2);
//                    operators.push(op::LIT);
//                    break;
//                case op::SUB:
//                    values.push_back(v1 - v2);
//                    operators.push(op::LIT);
//                    break;
//                case op::MUL:
//                    values.push_back(v1 * v2);
//                    operators.push(op::LIT);
//                    break;
//                case op::DIV:
//                    if (v2 == 0)
//                        v2 = 1;
//                    values.push_back(v1 / v2);
//                    operators.push(op::LIT);
//                    break;
//                case op::LIT:
//                    break;
//            }
//            std::cout << "\tresult: " << values.back() << std::endl;
//        }
//    }
//
//    std::cout << process.size() << std::endl;
//    std::cout << process.top() << std::endl;
//
//}
//
//struct silly
//{
//    long nyah = 50;
//    int bruh = 10;
//
//    friend std::ostream& operator<<(std::ostream& stream, const silly& silly)
//    {
//        stream << "[" << silly.nyah << " " << silly.bruh << "]";
//        return stream;
//    }
//};
//
//struct large
//{
//    unsigned char data[2048];
//};
//
//struct super_large
//{
//    unsigned char data[9582];
//};

blt::gp::stack_allocator alloc;

float nyah(float f, int i, bool b)
{
    return f + static_cast<float>(i * b);
}

int main()
{
//    constexpr blt::size_t MAX_ALIGNMENT = 8;
//    test();
//    std::cout << alignof(silly) << " " << sizeof(silly) << std::endl;
//    std::cout << alignof(super_large) << " " << sizeof(super_large) << " " << ((sizeof(super_large) + (MAX_ALIGNMENT - 1)) & ~(MAX_ALIGNMENT - 1))
//              << std::endl;
//    std::cout << ((sizeof(char) + (MAX_ALIGNMENT - 1)) & ~(MAX_ALIGNMENT - 1)) << " "
//              << ((sizeof(short) + (MAX_ALIGNMENT - 1)) & ~(MAX_ALIGNMENT - 1)) << std::endl;
//    std::cout << ((sizeof(int) + (MAX_ALIGNMENT - 1)) & ~(MAX_ALIGNMENT - 1)) << " " << ((sizeof(long) + (MAX_ALIGNMENT - 1)) & ~(MAX_ALIGNMENT - 1))
//              << std::endl;
//    std::cout << alignof(void*) << " " << sizeof(void*) << std::endl;
//    std::cout << blt::type_string<decltype(&"SillString")>() << std::endl;
//
//    alloc.push(50);
//    alloc.push(550.3f);
//    alloc.push(20.1230345);
//    alloc.push(true);
//    alloc.push(false);
//    alloc.push(std::string("SillyString"));
//    alloc.push(&"SillyString");
//
//    std::cout << std::endl;
//    std::cout << *alloc.pop<decltype(&"SillString")>() << std::endl;
//    std::cout << alloc.pop<std::string>() << std::endl;
//    std::cout << alloc.pop<bool>() << std::endl;
//    std::cout << alloc.pop<bool>() << std::endl;
//    std::cout << alloc.pop<double>() << std::endl;
//    std::cout << alloc.pop<float>() << std::endl;
//    std::cout << alloc.pop<int>() << std::endl;
//    std::cout << std::endl;
//
//    std::cout << "Is empty? " << alloc.empty() << std::endl;
//    alloc.push(silly{});
//    alloc.push(large{});
//    alloc.push(super_large{});
//    alloc.push(silly{25, 24});
//    alloc.push(large{});
//
//    std::cout << std::endl;
//    std::cout << "Is empty? " << alloc.empty() << std::endl;
//    alloc.pop<large>();
//    std::cout << "Is empty? " << alloc.empty() << std::endl;
//    std::cout << alloc.pop<silly>() << std::endl;
//    std::cout << "Is empty? " << alloc.empty() << std::endl;
//    alloc.pop<super_large>();
//    std::cout << "Is empty? " << alloc.empty() << std::endl;
//    alloc.pop<large>();
//    std::cout << "Is empty? " << alloc.empty() << std::endl;
//    std::cout << alloc.pop<silly>() << std::endl;
//    std::cout << std::endl;
//
//    std::cout << "Is empty? " << alloc.empty() << " bytes left: " << alloc.bytes_in_head() << std::endl;
//    std::cout << std::endl;
//
//    alloc.push(silly{2, 5});
//    alloc.push(large{});
//    alloc.push(super_large{});
//    alloc.push(silly{80, 10});
//    alloc.push(large{});
//    alloc.push(50);
//    alloc.push(550.3f);
//    alloc.push(20.1230345);
//    alloc.push(std::string("SillyString"));
    alloc.push(33.22f);
    alloc.push(120);
    alloc.push(true);
    
    blt::gp::operation<float, float, int, bool> silly_op(nyah);
    
    std::cout << silly_op(alloc) << std::endl;

//    std::cout << std::endl;
//
//    //auto* pointer = static_cast<void*>(head->metadata.offset);
//    //return std::align(alignment, bytes, pointer, remaining_bytes);
//
//    float f = 10.5;
//    int i = 412;
//    bool b = true;
//
//    std::array<void*, 3> arr{reinterpret_cast<void*>(&f), reinterpret_cast<void*>(&i), reinterpret_cast<void*>(&b)};
//
//    blt::span<void*, 3> spv{arr};
//
//    std::cout << silly_op.operator()(spv) << std::endl;
    
    //std::cout << "Hello World!" << std::endl;
}