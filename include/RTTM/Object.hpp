#ifndef RTTMOBJECT_H
#define RTTMOBJECT_H

#include <any>
#include <iostream>
#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>

#ifdef _MSC_VER
#else
    #include <cstdlib>
    #include <cxxabi.h>
    #include <cstdlib>
#endif

namespace RTTM
{
    template <typename T>
    using _Ref_ = std::shared_ptr<T>;

    template <typename T, typename... Args>
    _Ref_<T> Create_Ref_(Args&&... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    template <typename T>
    _Ref_<T> Create_Ref_2(T* ptr)
    {
        return _Ref_<T>(ptr, [](T* ptr)
        {
            std::cout << "delete: " << ptr << std::endl;
            delete ptr;
        });
    }

    template <typename T>
    _Ref_<T> AliasCreate(_Ref_<T> manager, T* ptr)
    {
        return std::shared_ptr<T>(manager, ptr);
    }

    static std::string Demangle(const char* mangledName)
    {
        static std::unordered_map<std::string, std::string> cache;
        static std::mutex cacheMutex;
        std::string mangled(mangledName);

        {
            std::lock_guard<std::mutex> lock(cacheMutex);
            auto it = cache.find(mangled);
            if (it != cache.end())
            {
                return it->second;
            }
        }

#ifdef _MSC_VER
        std::string name = mangled;
        const std::string to_remove = "class ";
        const std::string to_remove2 = "struct ";
        size_t pos = name.find(to_remove);
        if (pos != std::string::npos)
        {
            name.erase(pos, to_remove.length());
        }
        pos = name.find(to_remove2);
        if (pos != std::string::npos)
        {
            name.erase(pos, to_remove2.length());
        }
#else
        int status = -1;
        std::unique_ptr<char, void(*)(void*)> demangled {
            abi::__cxa_demangle(mangledName, nullptr, nullptr, &status),
            std::free
        };
        std::string name = (status == 0) ? demangled.get() : mangled;
#endif

        {
            std::lock_guard<std::mutex> lock(cacheMutex);
            cache[mangled] = name;
        }

        return name;
    }

    // 类型名称缓存系统
    namespace detail
    {
        // 单个类型的类型名称缓存
        template <typename T>
        struct TypeNameCache
        {
            static const std::string& get()
            {
                static const std::string name = Demangle(typeid(T).name());
                return name;
            }
        };

        // 组合类型的缓存系统
        template <typename... Types>
        struct CombinedTypeNameCache;

        // 特化：无参数
        template <>
        struct CombinedTypeNameCache<>
        {
            static const std::string& get()
            {
                static const std::string name = "default";
                return name;
            }
        };

        // 特化：单个参数
        template <typename T>
        struct CombinedTypeNameCache<T>
        {
            static const std::string& get()
            {
                return TypeNameCache<T>::get();
            }
        };

        // 特化：两个参数
        template <typename T1, typename T2>
        struct CombinedTypeNameCache<T1, T2>
        {
            static const std::string& get()
            {
                static const std::string name = TypeNameCache<T1>::get() + " " + TypeNameCache<T2>::get();
                return name;
            }
        };

        // 递归组合三个或更多参数
        template <typename T1, typename T2, typename... Rest>
        struct CombinedTypeNameCache<T1, T2, Rest...>
        {
            static const std::string& get()
            {
                static const std::string name = TypeNameCache<T1>::get() + " " + CombinedTypeNameCache<
                    T2, Rest...>::get();
                return name;
            }
        };
    }


    class Object
    {
    protected:
        std::any data;

    public:
        Object();
        explicit Object(std::any data);

        ~Object();


        /*
        template <typename First, typename... Rest>
        static std::string GetTypeName()
        {
            std::string result = Demangle(typeid(First).name());
            ((result += " " + Demangle(typeid(Rest).name())), ...);
            return result;
        }
        */

        template <typename... Types>
        static const std::string& GetTypeName()
        {
            return detail::CombinedTypeNameCache<Types...>::get();
        }

        template <typename T>
        T As();

        template <typename T>
        bool Is() const noexcept;

        std::string GetType() const;

        Object& operator=(const Object& other);

        template <typename T>
        Object& operator=(const T& value);

        std::ostream& operator<<(std::ostream& os);
    };

    template <typename T>
    T Object::As()
    {
        try
        {
            return std::any_cast<T>(data);
        }
        catch (const std::bad_any_cast& e)
        {
            throw std::runtime_error("Can not cast " + GetType() + " to " + Demangle(typeid(T).name()));
        }
    }


    template <typename T>
    Object& Object::operator=(const T& value)
    {
        data = value;
        return *this;
    }


    template <typename T>
    bool Object::Is() const noexcept
    {
        return data.type() == typeid(T);
    }
}


#endif //OBJECT_H
