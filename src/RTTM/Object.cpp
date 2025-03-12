#include "Object.hpp"

#include <mutex>
#include <unordered_map>

namespace RTTM
{

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
