#include <bits/char_traits.h>
#include <bits/alloc_traits.h>

template <class _CharT>
struct char_traits : public __gnu_cxx::char_traits<_CharT>
{
};

template <typename _Tp>
using _allocator_base = __gnu_cxx::new_allocator<_Tp>;

template <typename _Tp>
class allocator : public __allocator_base<_Tp>
{
};

template <typename _CharT, typename _Traits = char_traits<_CharT>, typename _Alloc = allocator<_CharT>>
class basic_string;

typedef basic_string<char> string;

template <typename _CharT, typename _Traits, typename _Alloc>
class basic_string
{
    typedef typename __gnu_cxx::__alloc_traits<_Alloc>::template rebind<_CharT>::other _Char_alloc_type;
    typedef __gnu_cxx::__alloc_traits<_Char_alloc_type> _Alloc_traits;

    // Types:
public:
    typedef _Char_alloc_type allocator_type;
    typedef typename _Alloc_traits::size_type size_type;
    typedef typename _Alloc_traits::pointer pointer;

    static const size_type npos = static_cast<size_type>(-1);

private:
    struct _Alloc_hider : allocator_type
    {
        pointer _M_p;
    };

    _Alloc_hider _M_dataplus;
    size_type _M_string_length;

    enum
    {
        _S_local_capacity = 15 / sizeof(_CharT)
    };

    union
    {
        _CharT _M_local_buf[_S_local_capacity + 1];
        size_type _M_allocated_capacity;
    };
};