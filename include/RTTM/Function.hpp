#ifndef RTTMFUNCTION_H
#define RTTMFUNCTION_H
#include <functional>
#include <stdexcept>

#include "Object.hpp"

namespace RTTM
{
    class Serializable;
}

namespace RTTM

{
    class IFunctionWrapper
    {
    public:
        std::string argumentTypes;
        virtual ~IFunctionWrapper();
    };

    template <typename R, typename... Args>
    class FunctionWrapper : public IFunctionWrapper
    {
        std::function<R(Args...)> func;

    public:
        FunctionWrapper(std::function<R(Args...)> f);

        FunctionWrapper(R (*f)(Args...));

        R operator()(Args... args);
    };

    template <typename R, typename... Args>
    FunctionWrapper<R, Args...>::FunctionWrapper(std::function<R(Args...)> f): func(std::move(f))
    {
        argumentTypes = Object::GetTypeName<Args...>();
    }

    template <typename R, typename... Args>
    FunctionWrapper<R, Args...>::FunctionWrapper(R (*f)(Args...)): func(f)
    {
        argumentTypes = Object::GetTypeName<Args...>();
    }

    template <typename R, typename... Args>
    R FunctionWrapper<R, Args...>::operator()(Args... args)
    {
        return func(std::forward<Args>(args)...);
    }

    class Function
    {
    private:
        std::shared_ptr<IFunctionWrapper> function;
        std::string name;
        void* instance = nullptr;
        bool isMemberFunction = false;

        template <typename... Args>
        std::string GetArgumentTypes();

    public:
        friend class Serializable;

        Function(const std::shared_ptr<IFunctionWrapper>& func, const std::string& name = "", void* instance = nullptr,
                 bool isMemberFunction = false);

        bool IsValid() const;

        bool operator==(const Function& other) const;

        Function();

        template <typename R, typename... Args>
        R Invoke(Args... args);

        std::string GetName() const;

        template <typename T, typename... Args>
        T operator()(Args... args);

        Function& operator=(const Function& other);
    };

    template <typename... Args>
    std::string Function::GetArgumentTypes()
    {
        std::string result;
        ((result += Demangle(typeid(Args).name()) + ", "), ...);
        if (!result.empty())
        {
            result.erase(result.size() - 2);
        }
        return result;
    }

    inline Function::Function(const std::shared_ptr<IFunctionWrapper>& func, const std::string& name, void* instance,
                              bool isMemberFunction): function(func), name(name), instance(instance),
                                                      isMemberFunction(isMemberFunction)
    {
    }

    inline bool Function::IsValid() const
    {
        return function != nullptr;
    }

    inline bool Function::operator==(const Function& other) const
    {
        return function == other.function;
    }

    inline Function::Function() = default;

    template <typename R, typename... Args>
    R Function::Invoke(Args... args)
    {
        if (isMemberFunction)
        {
            if (!instance)
                throw std::runtime_error("Instance is null for member function: " + name);
            auto wrapper = std::dynamic_pointer_cast<FunctionWrapper<R, void*, Args...>>(function);
            if (!wrapper)
                throw std::runtime_error(
                    "Function signature mismatch for: " + name + "Arguments are: " + GetArgumentTypes<Args...>());
            return (*wrapper)(instance, std::forward<Args>(args)...);
        }
        else
        {
            auto wrapper = std::dynamic_pointer_cast<FunctionWrapper<R, Args...>>(function);
            if (!wrapper)
                throw std::runtime_error(
                    "Function signature mismatch for: " + name + "Arguments are: " + GetArgumentTypes<Args...>());
            return (*wrapper)(std::forward<Args>(args)...);
        }
    }

    inline std::string Function::GetName() const
    {
        return name;
    }

    template <typename T, typename... Args>
    T Function::operator()(Args... args)
    {
        return Invoke<T>(std::forward<Args>(args)...);
    }

    inline Function& Function::operator=(const Function& other)
    {
        function = other.function;
        return *this;
    }
} // Cherry

#endif //FUNCTION_H
