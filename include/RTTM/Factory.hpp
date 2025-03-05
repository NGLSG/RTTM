#ifndef RTTMFACTORY_H
#define RTTMFACTORY_H
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
        //std::shared_ptr<Serializable> Create(Args... args);
        Serializable* Create(Args... args);
    };

    template <typename T, typename... Args>
    class Factory final : public IFactory, public std::enable_shared_from_this<Factory<T, Args...>>
    {
    private:
        using Allocator = std::allocator<T>;
        using Traits = std::allocator_traits<Allocator>;
        Allocator allocator;

    public:
        Factory();

        Serializable* Create(Args... args);

        void Destroy(T* ptr);
    };

    template <typename... Args>
    Serializable* IFactory::Create(Args... args)
    {
        return (*std::dynamic_pointer_cast<FunctionWrapper<Serializable*, Args...>>(createFunc))(
            std::forward<Args>(args)...);
        /*return (*std::dynamic_pointer_cast<FunctionWrapper<std::shared_ptr<Serializable>, Args...>>(createFunc))(
            std::forward<Args>(args)...);*/
    }

    /*    template <typename... Args>
    std::shared_ptr<Serializable> IFactory::Create(Args... args)
    {
        return (*std::dynamic_pointer_cast<FunctionWrapper<std::shared_ptr<Serializable>, Args...>>(createFunc))(
            std::forward<Args>(args)...);
    }*/

    template <typename T, typename... Args>
    Factory<T, Args...>::Factory()
    {
        /*createFunc = std::make_shared<FunctionWrapper<std::shared_ptr<Serializable>, Args...>>([this](Args... args)
        {
            return Create(std::forward<Args>(args)...);
        });*/
        createFunc = std::make_shared<FunctionWrapper<Serializable*, Args...>>([this](Args... args)
        {
            return Create(std::forward<Args>(args)...);
        });
    }

    /*template <typename T, typename... Args>
    std::shared_ptr<Serializable> Factory<T, Args...>::Create(Args... args) {
        // Allocate memory for one T object.
        T* ptr = Traits::allocate(allocator, 1);
        try {
            Traits::construct(allocator, ptr, std::forward<Args>(args)...);
        } catch (...) {
            Traits::deallocate(allocator, ptr, 1);
            throw;
        }

        Allocator allocCopy = allocator;

        auto deleter = [allocCopy](T* p) mutable {
            Traits::destroy(allocCopy, p);
            Traits::deallocate(allocCopy, p, 1);
        };

        std::shared_ptr<T> owner(ptr, deleter);

        // Return as a shared_ptr<Serializable> (assuming T derives from Serializable).
        return std::static_pointer_cast<Serializable>(owner);
    }*/

    template <typename T, typename... Args>
    Serializable* Factory<T, Args...>::Create(Args... args)
    {
        // Allocate memory for one T object.
        T* ptr = Traits::allocate(allocator, 1);
        try
        {
            Traits::construct(allocator, ptr, std::forward<Args>(args)...);
        }
        catch (...)
        {
            Traits::deallocate(allocator, ptr, 1);
            throw;
        }

        /*Allocator allocCopy = allocator;

        auto deleter = [allocCopy](T* p) mutable {
            Traits::destroy(allocCopy, p);
            Traits::deallocate(allocCopy, p, 1);
        };

        std::shared_ptr<T> owner(ptr, deleter);*/

        // Return as a shared_ptr<Serializable> (assuming T derives from Serializable).
        return static_cast<Serializable*>(ptr);
    }

    template <typename T, typename... Args>
    void Factory<T, Args...>::Destroy(T* ptr)
    {
        Traits::destroy(allocator, ptr);
        Traits::deallocate(allocator, ptr, 1);
    }
} // Cherry

#endif //FACTORY_H
