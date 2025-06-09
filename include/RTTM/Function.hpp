#ifndef RTTMFUNCTION_H
#define RTTMFUNCTION_H
#include <functional>
#include <stdexcept>
#include <type_traits>

#include "Object.hpp"

namespace RTTM
{
    class IFunctionWrapper
    {
    public:
        std::string argumentTypes;
        virtual ~IFunctionWrapper() = default;
    };

    template <typename R, typename... Args>
    class FunctionWrapper : public IFunctionWrapper
    {
        std::function<R(Args...)> func;

    public:
        // 默认构造函数
        FunctionWrapper() = default;

        // 接受 std::function 的构造函数
        explicit FunctionWrapper(const std::function<R(Args...)>& f) : func(f)
        {
            argumentTypes = Object::GetTypeName<Args...>();
        }

        // 接受 std::function 的移动构造函数
        explicit FunctionWrapper(std::function<R(Args...)>&& f) : func(std::move(f))
        {
            argumentTypes = Object::GetTypeName<Args...>();
        }

        // 接受函数指针的构造函数
        explicit FunctionWrapper(R (*f)(Args...)) : func(f)
        {
            argumentTypes = Object::GetTypeName<Args...>();
        }

        // 针对 C++20 的通用可调用对象构造函数
        template<typename F>
        explicit FunctionWrapper(F&& f)
            : func(std::forward<F>(f))
        {
            static_assert(std::is_invocable_r_v<R, F, Args...>, "Callable object must be invocable with given signature");
            static_assert(!std::is_same_v<std::decay_t<F>, FunctionWrapper>, "Cannot construct from same type");
            static_assert(!std::is_same_v<std::decay_t<F>, std::function<R(Args...)>>, "Use specific std::function constructor");
            argumentTypes = Object::GetTypeName<Args...>();
        }

        // 复制构造函数
        FunctionWrapper(const FunctionWrapper& other) = default;

        // 移动构造函数
        FunctionWrapper(FunctionWrapper&& other) noexcept = default;

        // 复制赋值运算符
        FunctionWrapper& operator=(const FunctionWrapper& other) = default;

        // 移动赋值运算符
        FunctionWrapper& operator=(FunctionWrapper&& other) noexcept = default;

        // 析构函数
        ~FunctionWrapper() = default;

        // 调用运算符
        R operator()(Args... args)
        {
            return func(std::forward<Args>(args)...);
        }

        // 检查是否有效
        bool IsValid() const
        {
            return static_cast<bool>(func);
        }
    };

    class Function
    {
    private:
        std::shared_ptr<IFunctionWrapper> function;
        std::string name;
        void* instance = nullptr;
        bool isMemberFunction = false;

        template <typename... Args>
        std::string getArgumentTypes()
        {
            std::string result;
            ((result += Demangle(typeid(Args).name()) + ", "), ...);
            if (!result.empty())
            {
                result.erase(result.size() - 2);
            }
            return result;
        }

    public:
        friend class Serializable;

        Function() = default;

        Function(const std::shared_ptr<IFunctionWrapper>& func, const std::string& name = "", void* instance = nullptr,
                 bool isMemberFunction = false) : function(func), name(name), instance(instance),
                                                  isMemberFunction(isMemberFunction)
        {
        }

        bool IsValid() const
        {
            return function != nullptr;
        }

        bool operator==(const Function& other) const
        {
            return function == other.function;
        }

        template <typename R, typename... Args>
        R Invoke(Args... args)
        {
            if (isMemberFunction)
            {
                if (!instance)
                    throw std::runtime_error("Instance is null for member function: " + name);
                auto wrapper = std::dynamic_pointer_cast<FunctionWrapper<R, void*, Args...>>(function);
                if (!wrapper)
                    throw std::runtime_error(
                        "Function signature mismatch for: " + name + "Arguments are: " + getArgumentTypes<Args...>());
                return (*wrapper)(instance, std::forward<Args>(args)...);
            }
            else
            {
                auto wrapper = std::dynamic_pointer_cast<FunctionWrapper<R, Args...>>(function);
                if (!wrapper)
                    throw std::runtime_error(
                        "Function signature mismatch for: " + name + "Arguments are: " + getArgumentTypes<Args...>());
                return (*wrapper)(std::forward<Args>(args)...);
            }
        }

        std::string GetName() const
        {
            return name;
        }

        template <typename T, typename... Args>
        T operator()(Args... args)
        {
            return Invoke<T>(std::forward<Args>(args)...);
        }

        Function& operator=(const Function& other)
        {
            function = other.function;
            return *this;
        }
    };
} // RTTM

#endif //RTTMFUNCTION_H