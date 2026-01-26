/********************************************************************\
 * except-fence.h - Exception fencing for C/C++ interface           *
 * Copyright (C) 2026, Russ Gorby                                   *
 *                                                                  *
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, write to the Free Software      *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.        *
\********************************************************************/

#ifndef EXCEPT_FENCE_HPP
#define EXCEPT_FENCE_HPP

#include <string>
#include <typeinfo>
#include <string_view>
#include <cstdint>

constexpr uint8_t DEFAULT_EXCEPTION_ERROR_VALUE = 0x7e; // works for all int types

struct ExceptData {
    bool exception_hit;
    const std::type_info* exception_type;
    std::string exception_message;

    ExceptData() {
        reset();
    }

    // Copy constructor
    ExceptData(const ExceptData& other)
    {
        exception_hit = other.exception_hit;
        exception_type = other.exception_type;
        exception_message = other.exception_message;
    }

    bool operator==(const ExceptData& other) const {
        return exception_hit == other.exception_hit &&
            exception_type == other.exception_type &&
            exception_message == other.exception_message;
    }

    void reset() {
        exception_hit = false;
        exception_type = nullptr;
        exception_message.clear();
    }
};

class ExceptFence
{
    private:
        unsigned long long m_exception_error_integer{DEFAULT_EXCEPTION_ERROR_VALUE};
        ExceptData m_except_data;

        void reset()
        {
            m_except_data.reset();
        }

    public:
        // CTOR
        ExceptFence()
        {
            reset();
        }

        template<typename T>
        ExceptFence(T error_value)
        {
            reset();
            if constexpr (std::is_integral_v<T>)
            {
                if (error_value != 0)
                {
                    /*
                     * for integer values we can define the error value returned
                     * when an exception occurs
                     * for other types we can only return nullptr, or zeroed out structure
                     */
                    m_exception_error_integer = static_cast<unsigned long long>(error_value);
                }
            }
        }

        template<typename Func, typename... Args>
        auto forward_to(Func&& func, Args&&... args)
        {
            try {
                m_except_data.exception_hit = true;
                auto result = func(std::forward<Args>(args)...);
                m_except_data.exception_hit = false;
                return result;
            }
            catch (const std::exception& ex) {
                m_except_data.exception_type = &typeid(ex);
                m_except_data.exception_message = ex.what();
            }
            using ReturnType = std::invoke_result_t<Func, Args...>;
            if constexpr (std::is_integral_v<ReturnType>) {
                return static_cast<ReturnType>(m_exception_error_integer);
            } else if constexpr (std::is_pointer_v<ReturnType>) {
                return static_cast<ReturnType>(nullptr);
            } else {
                return ReturnType{};
            }
        }

        template<typename Func, typename... Args>
        void forward_to_void(Func&& func, Args&&... args)
        {
            try {
                m_except_data.exception_hit = true;
                (void) func(std::forward<Args>(args)...);
                m_except_data.exception_hit = false;
            }
            catch (const std::exception& ex) {
                m_except_data.exception_type = &typeid(ex);
                m_except_data.exception_message = ex.what();
            }
        }

        void get_except_data(ExceptData& data)
        {
            data = m_except_data;
        }
};

constexpr bool str_equal(std::string_view a, std::string_view b)
{
    return a == b;
}

#define STRIP_PARENS(x) STRIP_PARENS_IMPL x
#define STRIP_PARENS_IMPL(...) __VA_ARGS__

/*
 * macros to wrap C-APIs and install an exception fence around them
 * these are intended to replace the function definitions
 * This way we can convert the C-code to C++ but maintain the C-APIs
 * and prevent exceptions from bleeding into the C-code
 *
 * there are 4 macros to handle all the combinations of
 * void args and void return
 *
 * Params - fully typed parameter list in parenthesis
 *          e.g. (const char *arg1, unsigned short arg2)
 * Args - same as Params but without the types
 *          e.g. (arg1, arg2)
 *
 * Example
 * const char *c_api_01(const char *arg1, unsigned short arg2)
 * will be replaced with
 * SAFE_C_API_ARGS(const char *, c_api_01, (const char *arg1, unsigned short arg2), (arg1, arg2))
 */
#define SAFE_C_API_ARGS(RetType, FuncName, Params, Args) \
static RetType loc_##FuncName Params; \
extern "C" RetType FuncName Params \
{ \
    ExceptFence exf; \
    static_assert(! str_equal(#RetType, "void")); \
    return exf.forward_to(loc_##FuncName, STRIP_PARENS(Args)); \
} \
\
static RetType loc_##FuncName Params

#define SAFE_C_API_NOARGS(RetType, FuncName) \
static RetType loc_##FuncName (void); \
extern "C" RetType FuncName (void) \
{ \
    ExceptFence exf; \
    return exf.forward_to(loc_##FuncName); \
} \
\
static RetType loc_##FuncName (void)

#define SAFE_C_API_VOID_ARGS(FuncName, Params, Args) \
static void loc_##FuncName Params; \
extern "C" void FuncName Params \
{ \
    ExceptFence exf; \
    exf.forward_to_void(loc_##FuncName, STRIP_PARENS(Args)); \
} \
\
static void loc_##FuncName Params

#define SAFE_C_API_VOID_NOARGS(FuncName) \
static void loc_##FuncName (void); \
extern "C" void FuncName (void) \
{ \
    ExceptFence exf; \
    exf.forward_to_void(loc_##FuncName); \
} \
\
static void loc_##FuncName (void)


#endif // EXCEPT_FENCE_HPP