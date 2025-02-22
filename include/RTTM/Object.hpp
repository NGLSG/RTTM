#ifndef OBJECT_H
#define OBJECT_H

#include <any>
#include <iostream>
#include <string>
#include <memory>

#ifdef _MSC_VER
#else
    #include <cstdlib>
    #include <cxxabi.h>
    #include <cstdlib>
#endif

namespace RTTM
{
    class Object
    {
    protected:
        std::any data;

    public:
        Object();
        explicit Object(std::any data);

        ~Object();

        static std::string Demangle(const char* mangledName);

        template <typename T>
        static std::string GetTypeName();

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
    std::string Object::GetTypeName()
    {
        return Demangle(typeid(T).name());
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
