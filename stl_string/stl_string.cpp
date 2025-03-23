#include <string>
#include <iostream>
#include <typeinfo>
#include <boost/type_index.hpp>

int main()
{
    union Union
    {
        char _M_local_buf[16];
        unsigned long _M_allocated_capacity;
    } u;
    std::string s {"1234567890987654321234"};
    std::cout << sizeof(u) << std::endl;
    std::cout << boost::typeindex::type_id<std::string::pointer>().pretty_name() << std::endl;
    return 0;
}