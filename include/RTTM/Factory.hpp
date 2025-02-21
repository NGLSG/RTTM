#ifndef FACTORY_H
#define FACTORY_H
#include <cstdarg>
#include <functional>
#include <memory>
#include <unordered_map>

#include "Function.hpp"


namespace RTTM
{
    class Serializable;

    class IFactory
    {
    protected:
        std::shared_ptr<IFunctionWrapper> createFunc;

    public:
        virtual ~IFactory() = default;

        template <typename... Args>
        std::shared_ptr<Serializable> Create(Args... args);
    };

    template <typename T, typename... Args>
    class Factory final : public IFactory
    {
    private:
        using Allocator = std::allocator<T>;
        using Traits = std::allocator_traits<Allocator>;
        Allocator allocator;

    public:
        Factory();

        std::shared_ptr<Serializable> Create(Args... args);

        void Destroy(T* ptr);
    };

    template <typename... Args>
    std::shared_ptr<Serializable> IFactory::Create(Args... args)
    {
        return (*std::dynamic_pointer_cast<FunctionWrapper<std::shared_ptr<Serializable>, Args...>>(createFunc))(
            std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    Factory<T, Args...>::Factory()
    {
        createFunc = std::make_shared<FunctionWrapper<std::shared_ptr<Serializable>, Args...>>([this](Args... args)
        {
            return Create(std::forward<Args>(args)...);
        });
    }

    template <typename T, typename... Args>
    std::shared_ptr<Serializable> Factory<T, Args...>::Create(Args... args)
    {
        T* ptr = Traits::allocate(allocator, 1);
        Traits::construct(allocator, ptr, std::forward<Args>(args)...);
        return std::shared_ptr<T>(ptr, [this](T* p)
        {
            Traits::destroy(allocator, p);
            Traits::deallocate(allocator, p, 1);
        });
    }

    template <typename T, typename... Args>
    void Factory<T, Args...>::Destroy(T* ptr)
    {
        Traits::destroy(allocator, ptr);
        Traits::deallocate(allocator, ptr, 1);
    }
} // Cherry

#endif //FACTORY_H
