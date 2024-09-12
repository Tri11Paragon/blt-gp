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
#include <blt/std/logging.h>
#include <variant>
#include <stack>
#include <deque>
#include <vector>
#include <random>

//// small scale
enum class op
{
    ADD,
    SUB,
    MUL,
    DIV,
    LIT
};

std::string to_string(op o)
{
    switch (o)
    {
        case op::ADD:
            return "ADD";
        case op::SUB:
            return "SUB";
        case op::MUL:
            return "MUL";
        case op::DIV:
            return "DIV";
        case op::LIT:
            return "LIT";
    }
    return "";
}

constexpr static long SEED = 41912;

op generate_op()
{
    static std::mt19937_64 engine(SEED);
    static std::uniform_int_distribution dist(0, static_cast<int>(op::LIT) - 1);
    return static_cast<op>(dist(engine));
}

bool choice()
{
    static std::mt19937_64 engine(SEED);
    static std::uniform_int_distribution dist(0, 1);
    return dist(engine);
}

float random_value()
{
    static std::mt19937_64 engine(SEED);
    static std::uniform_real_distribution dist(0.0f, 10.0f);
    return dist(engine);
}

void test()
{
    std::vector<op> operations;
    std::vector<float> values;
    
    std::stack<op> tree_generator;
    tree_generator.push(generate_op());
    
    while (!tree_generator.empty())
    {
        auto opn = tree_generator.top();
        tree_generator.pop();
        
        operations.push_back(opn);
        if (opn == op::LIT)
        {
            values.push_back(random_value());
            continue;
        }
        
        // child 1
        if (choice())
            tree_generator.push(generate_op());
        else
            tree_generator.push(op::LIT);
        
        // child 2
        if (choice())
            tree_generator.push(generate_op());
        else
            tree_generator.push(op::LIT);
    }
    
    // print out the tree / operators
    for (const auto& v : operations)
        std::cout << to_string(v) << " ";
    std::cout << std::endl;
    
    {
        std::stack<blt::size_t> process;
        for (const auto& v : operations)
        {
            switch (v)
            {
                case op::ADD:
                case op::SUB:
                case op::MUL:
                case op::DIV:
                    process.emplace(2);
                    std::cout << "(";
                    break;
                case op::LIT:
                    break;
            }
            std::cout << to_string(v);
            while (!process.empty())
            {
                auto top = process.top();
                process.pop();
                if (top == 0)
                {
                    std::cout << ")";
                    continue;
                } else
                {
                    std::cout << " ";
                    process.push(top - 1);
                    break;
                }
            }
        }
        while (!process.empty())
        {
            auto top = process.top();
            process.pop();
            if (top == 0)
            {
                std::cout << ") ";
                continue;
            } else
            {
                std::cerr << "FUCK YOU\n";
                break;
            }
        }
        std::cout << std::endl;
    }
    
    for (const auto& v : values)
        std::cout << v << " ";
    std::cout << std::endl;
    
    {
        std::stack<blt::size_t> process;
        blt::size_t index = 0;
        for (const auto& v : operations)
        {
            switch (v)
            {
                case op::ADD:
                case op::SUB:
                case op::MUL:
                case op::DIV:
                    process.emplace(2);
                    std::cout << "(";
                    std::cout << to_string(v);
                    break;
                case op::LIT:
                    std::cout << values[index++];
                    break;
            }
            
            while (!process.empty())
            {
                auto top = process.top();
                process.pop();
                if (top == 0)
                {
                    std::cout << ")";
                    continue;
                } else
                {
                    std::cout << " ";
                    process.push(top - 1);
                    break;
                }
            }
        }
        while (!process.empty())
        {
            auto top = process.top();
            process.pop();
            if (top == 0)
            {
                std::cout << ") ";
                continue;
            } else
            {
                std::cerr << "FUCK YOU\n";
                break;
            }
        }
        std::cout << std::endl;
    }
    
    // run the tree
    std::stack<float> process;
    std::stack<op> operators;
    
    for (const auto& v : operations)
        operators.push(v);
    
    while (!operators.empty())
    {
        auto oper = operators.top();
        operators.pop();
        if (oper == op::LIT)
        {
            process.push(values.back());
            values.pop_back();
        } else
        {
            auto v1 = process.top();
            process.pop();
            auto v2 = process.top();
            process.pop();
            std::cout << "processing oper " << to_string(oper) << " with values " << v1 << " " << v2 << std::endl;
            switch (oper)
            {
                case op::ADD:
                    values.push_back(v1 + v2);
                    operators.push(op::LIT);
                    break;
                case op::SUB:
                    values.push_back(v1 - v2);
                    operators.push(op::LIT);
                    break;
                case op::MUL:
                    values.push_back(v1 * v2);
                    operators.push(op::LIT);
                    break;
                case op::DIV:
                    if (v2 == 0)
                        v2 = 1;
                    values.push_back(v1 / v2);
                    operators.push(op::LIT);
                    break;
                case op::LIT:
                    break;
            }
            std::cout << "\tresult: " << values.back() << std::endl;
        }
    }
    
    std::cout << process.size() << std::endl;
    std::cout << "Total Results: " << process.top() << std::endl;
    
}

float nyah(float a, int b, bool c)
{
    return a + static_cast<float>(b) * c;
}

struct bytes_16_struct
{
    unsigned long bruh;
    int nya;
    
    friend std::ostream& operator<<(std::ostream& out, const bytes_16_struct& s)
    {
        out << "[" << s.bruh << " " << s.nya << "]";
        return out;
    }
};

struct bytes_256_struct
{
    unsigned char data[256];
};

struct bytes_5129_struct
{
    unsigned char data[5129];
};

struct bytes_4096_page_struct
{
    unsigned char data[4096 - 32];
};

struct context
{
    float x, y;
};

namespace blt::gp::detail
{
    class operator_storage_test
    {
        public:
            explicit operator_storage_test(blt::gp::operator_builder<context>& ops): ops(ops)
            {}
            
            inline blt::gp::detail::operator_func_t& operator[](blt::size_t index)
            {
                return ops.storage.operators[index].func;
            }
        
        private:
            blt::gp::operator_builder<context>& ops;
    };
}

blt::gp::stack_allocator alloc;

int main()
{
    constexpr blt::size_t MAX_ALIGNMENT = 8;
    test();
    std::cout << alignof(bytes_16_struct) << " " << sizeof(bytes_16_struct) << std::endl;
    std::cout << alignof(bytes_5129_struct) << " " << sizeof(bytes_5129_struct) << " " << ((sizeof(bytes_5129_struct) + (MAX_ALIGNMENT - 1)) & ~(MAX_ALIGNMENT - 1))
              << std::endl;
    std::cout << ((sizeof(char) + (MAX_ALIGNMENT - 1)) & ~(MAX_ALIGNMENT - 1)) << " "
              << ((sizeof(short) + (MAX_ALIGNMENT - 1)) & ~(MAX_ALIGNMENT - 1)) << std::endl;
    std::cout << ((sizeof(int) + (MAX_ALIGNMENT - 1)) & ~(MAX_ALIGNMENT - 1)) << " " << ((sizeof(long) + (MAX_ALIGNMENT - 1)) & ~(MAX_ALIGNMENT - 1))
              << std::endl;
    std::cout << alignof(void*) << " " << sizeof(void*) << std::endl;
    std::cout << blt::type_string<decltype(&"SillString")>() << std::endl;
    
    alloc.push(50);
    alloc.push(550.3f);
    alloc.push(20.1230345);
    alloc.push(true);
    alloc.push(false);
    //alloc.push(std::string("SillyString"));
    alloc.push(&"SillyString");
    
    std::cout << std::endl;
    std::cout << *alloc.pop<decltype(&"SillString")>() << std::endl;
    //std::cout << alloc.pop<std::string>() << std::endl;
    std::cout << alloc.pop<bool>() << std::endl;
    std::cout << alloc.pop<bool>() << std::endl;
    std::cout << alloc.pop<double>() << std::endl;
    std::cout << alloc.pop<float>() << std::endl;
    std::cout << alloc.pop<int>() << std::endl;
    std::cout << std::endl;
    
    std::cout << "Is empty? " << alloc.empty() << std::endl;
    
    alloc.push(bytes_4096_page_struct{});
    std::cout << "Used bytes: " << alloc.size() << std::endl;
    alloc.push(bytes_16_struct{});
    std::cout << "Used bytes: " << alloc.size() << std::endl;
    alloc.pop<bytes_16_struct>();
    std::cout << "Used bytes: " << alloc.size() << std::endl;
    alloc.pop<bytes_4096_page_struct>();
    std::cout << "Used bytes: " << alloc.size() << std::endl;
    
    std::cout << std::endl;
    std::cout << "Is empty? " << alloc.empty() << " " << alloc.size() << std::endl;
    std::cout << std::endl;
    
    alloc.push(bytes_16_struct{});
    std::cout << "Used bytes: " << alloc.size() << std::endl;
    alloc.push(bytes_256_struct{});
    std::cout << "Used bytes: " << alloc.size() << std::endl;
    alloc.push(bytes_5129_struct{});
    std::cout << "Used bytes: " << alloc.size() << std::endl;
    alloc.push(bytes_16_struct{25, 24});
    std::cout << "Used bytes: " << alloc.size() << std::endl;
    alloc.push(bytes_256_struct{});
    std::cout << "Used bytes: " << alloc.size() << std::endl;
    
    std::cout << std::endl;
    std::cout << "Is empty? " << alloc.empty() << " " << alloc.size() << std::endl;
    alloc.pop<bytes_256_struct>();
    std::cout << "Is empty? " << alloc.empty() << " " << alloc.size() << std::endl;
    std::cout << alloc.pop<bytes_16_struct>() << std::endl;
    std::cout << "Is empty? " << alloc.empty() << " " << alloc.size() << std::endl;
    alloc.pop<bytes_5129_struct>();
    std::cout << "Is empty? " << alloc.empty() << " " << alloc.size() << std::endl;
    alloc.pop<bytes_256_struct>();
    std::cout << "Is empty? " << alloc.empty() << " " << alloc.size() << std::endl;
    std::cout << alloc.pop<bytes_16_struct>() << std::endl;
    std::cout << std::endl;
    
    
    std::cout << "Is empty? " << alloc.empty() << " bytes left: " << alloc.bytes_in_head() << std::endl;
    std::cout << std::endl;
    
    alloc.push(bytes_16_struct{2, 5});
    alloc.push(bytes_256_struct{});
    alloc.push(bytes_5129_struct{});
    alloc.push(bytes_16_struct{80, 10});
    alloc.push(bytes_256_struct{});
    alloc.push(50);
    alloc.push(550.3f);
    alloc.push(20.1230345);
    //alloc.push(std::string("SillyString"));
    alloc.push(33.22f);
    alloc.push(120);
    alloc.push(true);
    
    blt::gp::operation_t silly_op(nyah);
    blt::gp::operation_t silly_op_2([](float f, float g) {
        return f + g;
    });
    
    std::cout << silly_op(alloc) << std::endl;
    
    std::cout << "Is empty? " << alloc.empty() << std::endl;
    
    std::cout << std::endl;
    
    blt::gp::operation_t silly_op_3([](const context& ctx, float f) {
        return ctx.x + ctx.y + f;
    });
    
    blt::gp::operation_t silly_op_4([](const context& ctx) {
        return ctx.x;
    });
    
    blt::gp::operator_builder<context> ops{};
    
    //BLT_TRACE(blt::type_string<decltype(silly_op_3)::first::type>());
    //BLT_TRACE(typeid(decltype(silly_op_3)::first::type).name());
    //BLT_TRACE(blt::type_string<blt::gp::detail::remove_cv_ref<decltype(silly_op_3)::first::type>>());
    //BLT_TRACE("Same types? %s", (std::is_same_v<context, blt::gp::detail::remove_cv_ref<decltype(silly_op_3)::first::type>>) ? "true" : "false");
    
    ops.build(silly_op_3, silly_op_4, silly_op_2);
    blt::gp::detail::operator_storage_test de(ops);
    
    context hello{5, 10};
    
    alloc.push(1.153f);
    de[0](static_cast<void*>(&hello), alloc, alloc);
    BLT_TRACE("first value: %f", alloc.pop<float>());
    
    de[1](static_cast<void*>(&hello), alloc, alloc);
    BLT_TRACE("second value: %f", alloc.pop<float>());
    
    alloc.push(1.0f);
    alloc.push(52.213f);
    de[2](static_cast<void*>(&hello), alloc, alloc);
    BLT_TRACE("third value: %f", alloc.pop<float>());
    
    //auto* pointer = static_cast<void*>(head->metadata.offset);
    //return std::align(alignment, bytes, pointer, remaining_bytes);
    
    float f = 10.5;
    int i = 412;
    bool b = true;
    
    alloc.push(f);
    alloc.push(i);
    alloc.push(b);
    
    //std::array<void*, 3> arr{reinterpret_cast<void*>(&f), reinterpret_cast<void*>(&i), reinterpret_cast<void*>(&b)};
    
    //blt::span<void*, 3> spv{arr};
    
    std::cout << silly_op.operator()(alloc) << std::endl;
    
    std::cout << "Hello World!" << std::endl;
    
    return 0;
}