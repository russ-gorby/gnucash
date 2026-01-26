#include <gtest/gtest.h>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include "except-fence.hpp"

bool do_throw = false;
bool void_func_completed = false;
struct struct_api_args {
    bool a;
    int b;
    const char *c;
    bool operator==(const struct_api_args& other) const {
        return a == other.a &&
            b == other.b &&
            c == other.c;
    }
};

// SAFE_C_API_ARGS
extern "C" const char *api_args1([[maybe_unused]] float a, [[maybe_unused]] uint16_t b);
extern "C" uint64_t api_args2([[maybe_unused]] const char *a, [[maybe_unused]] double b);
extern "C" struct_api_args api_args3([[maybe_unused]] const char *a, [[maybe_unused]] double b);

// SAFE_C_API_ARGS returning pointer
const char *api_args1_return_default = "this is the way";
SAFE_C_API_ARGS(const char *, api_args1,
    ([[maybe_unused]] float a, [[maybe_unused]] uint16_t b),
    (a, b))
{
    if (do_throw)
    {
        throw std::invalid_argument("bad arg");
    }
    return api_args1_return_default;
}

// SAFE_C_API_ARGS returning integer
uint64_t api_args2_return_default = 787878;
SAFE_C_API_ARGS(uint64_t, api_args2,
    ([[maybe_unused]] const char *a, [[maybe_unused]] double b),
    (a, b))
{
    if (do_throw)
    {
        throw std::out_of_range("way gone");
    }
    return api_args2_return_default;
}

// SAFE_C_API_ARGS returning structure
struct struct_api_args api_args3_return_default = {
    true, 678, "which way?"
};
SAFE_C_API_ARGS(struct_api_args, api_args3,
    ([[maybe_unused]] const char *a, [[maybe_unused]] double b),
    (a, b))
{
    if (do_throw)
    {
        throw std::overflow_error("cup is too full");
    }
    return api_args3_return_default;
}


// SAFE_C_API_NOARGS
extern "C" const char *api_args4(void);
extern "C" uint64_t api_args5(void);
extern "C" struct_api_args api_args6(void);

// SAFE_C_API_NOARGS returning pointer
const char *api_args4_return_default = "this is the new way";
SAFE_C_API_NOARGS(const char *, api_args4)
{
    if (do_throw)
    {
        throw std::invalid_argument("bad arg");
    }
    return api_args4_return_default;
}

// SAFE_C_API_NOARGS returning integer
uint64_t api_args5_return_default = 676767;
SAFE_C_API_NOARGS(uint64_t, api_args5)
{
    if (do_throw)
    {
        throw std::out_of_range("way gone");
    }
    return api_args5_return_default;
}

// SAFE_C_API_NOARGS returning structure
struct struct_api_args api_args6_return_default = {
    false, 345, "wait, which way?"
};
SAFE_C_API_NOARGS(struct_api_args, api_args6)
{
    if (do_throw)
    {
        throw std::overflow_error("cup is too full");
    }
    return api_args6_return_default;
}

// SAFE_C_API_VOID_ARGS
extern "C" void api_args7([[maybe_unused]] float a, [[maybe_unused]] uint16_t b);

SAFE_C_API_VOID_ARGS(api_args7,
    ([[maybe_unused]] float a, [[maybe_unused]] uint16_t b),
    (a, b))
{
    void_func_completed = false;
    if (do_throw)
    {
        throw std::invalid_argument("bad arg");
    }
    void_func_completed = true;
}

// SAFE_C_API_VOID_NOARGS
extern "C" void api_args8(void);

SAFE_C_API_VOID_NOARGS(api_args8)
{
    void_func_completed = false;
    if (do_throw)
    {
        throw std::overflow_error("cup is too full");
    }
    void_func_completed = true;
}

TEST(c_api_fence, safe_c_api_args) {
    // no-throw cases
    do_throw = false;

    const char *str1 = api_args1(1.23, 123);
    ASSERT_EQ(str1, api_args1_return_default);

    uint64_t res2 = api_args2("foo", 12345.78);
    ASSERT_EQ(res2, api_args2_return_default);

    struct struct_api_args res3 = api_args3("bar", 67.89778);
    ASSERT_EQ(res3, api_args3_return_default);

    // throw cases
    do_throw = true;

    str1 = api_args1(1.23, 123);
    ASSERT_EQ(str1, nullptr);

    res2 = api_args2("foo", 12345.78);
    ASSERT_EQ(res2, static_cast<uint64_t>(DEFAULT_EXCEPTION_ERROR_VALUE));

    res3 = api_args3("bar", 67.89778);
    ASSERT_EQ(res3, struct_api_args{});
}

TEST(c_api_fence, safe_c_api_noargs) {
    // no-throw cases
    do_throw = false;

    const char *str4 = api_args4();
    ASSERT_EQ(str4, api_args4_return_default);

    uint64_t res5 = api_args5();
    ASSERT_EQ(res5, api_args5_return_default);

    struct struct_api_args res6 = api_args6();
    ASSERT_EQ(res6, api_args6_return_default);

    // throw cases
    do_throw = true;

    str4 = api_args4();
    ASSERT_EQ(str4, nullptr);

    res5 = api_args5();
    ASSERT_EQ(res5, static_cast<uint64_t>(DEFAULT_EXCEPTION_ERROR_VALUE));

    res6 = api_args6();
    ASSERT_EQ(res6, struct_api_args{});
}

TEST(c_api_fence, safe_c_api_void_args) {
    // no-throw cases
    do_throw = false;

    api_args7(45.678, 12);
    ASSERT_TRUE(void_func_completed);

    // no-throw cases
    do_throw = true;

    api_args7(45.678, 12);
    ASSERT_FALSE(void_func_completed);
}

TEST(c_api_fence, safe_c_api_void_noargs) {
    // no-throw cases
    do_throw = false;

    api_args8();
    ASSERT_TRUE(void_func_completed);

    // no-throw cases
    do_throw = true;

    api_args8();
    ASSERT_FALSE(void_func_completed);
}
