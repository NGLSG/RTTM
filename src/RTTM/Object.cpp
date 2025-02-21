#include "Object.hpp"

namespace RTTM
{
    std::string Object::Demangle(const char* mangledName)
    {
#ifdef _MSC_VER
        std::string name = mangledName;
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

        return name;
#else
        int status = -1;
        // abi::__cxa_demangle returns a pointer to a C string that must be freed with free()
        std::unique_ptr<char, void(*)(void*)> demangled{
            abi::__cxa_demangle(mangledName, nullptr, nullptr, &status),
            std::free
        };
        return (status == 0) ? demangled.get() : mangledName;
#endif
    }

    Object::Object() = default;

    Object::~Object()
    {
        data.reset();
    }

    std::ostream& Object::operator<<(std::ostream& os)
    {
        if (Is<int>())
        {
            return os << As<int>();
        }
        if (Is<float>())
        {
            return os << As<float>();
        }
        if (Is<double>())
        {
            return os << As<double>();
        }
        if (Is<std::string>())
        {
            return os << As<std::string>();
        }
        if (Is<char>())
        {
            return os << As<char>();
        }
        if (Is<bool>())
        {
            return os << As<bool>();
        }
        if (Is<double>())
        {
            return os << As<double>();
        }
        return os << GetType();
    }

    Object::Object(std::any data): data(std::move(data))
    {
    }

    std::string Object::GetType() const
    {
        return Demangle(data.type().name());
    }

    Object& Object::operator=(const Object& other)
    {
        data = other.data;
        return *this;
    }
}
