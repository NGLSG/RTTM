#include "Serializable.hpp"

namespace RTTM
{
    IEnum IEnum::GetEnumByName(const std::string& name)
    {
        if (SerializableVar::Enums.find(name) == SerializableVar::Enums.end())
        {
            throw std::runtime_error("The enum has not been registered: " + name);
        }
        IEnum newEnum;
        Ref<IEnum> refEnum = SerializableVar::Enums[name];
        newEnum.Members = refEnum->Members;
        return newEnum;
    }

    std::vector<std::string> IEnum::GetRegisteredEnum()
    {
        std::vector<std::string> res;
        for (auto& it : SerializableVar::Enums)
        {
            res.push_back(it.first);
        }
        return res;
    }

    Serializable& Serializable::operator=(const Serializable& other)
    {
        Members = other.Members;
        Methods = other.Methods;
        MembersOffset = other.MembersOffset;
        return *this;
    }

    Function Serializable::GetMethod(const std::string& name)
    {
        auto it = Methods.find(name);
        if (it == Methods.end())
            throw std::runtime_error("Function not found: " + name);
        auto f = Function(it->second, name);
        f.instance = this;
        f.isMemberFunction = true;
        return f;
    }

    std::vector<std::string> Serializable::GetPropertyNames()
    {
        std::vector<std::string> names;
        for (const auto& [name, value] : Members)
        {
            names.push_back(name);
        }
        return names;
    }

    std::vector<std::string> Serializable::GetFunctionNames()
    {
        std::vector<std::string> names;
        for (const auto& [name, value] : Methods)
        {
            names.push_back(name);
        }
        return names;
    }

    Ref<Type> Serializable::GetProperty(const std::string& name)
    {
        auto it = Members.find(name);
        if (it == Members.end())
            throw std::runtime_error("Property not found: " + name);
        return it->second.As<Ref<Type>>();
    }


    Serializable Serializable::DeepClone()
    {
        return *this;
    }

    std::vector<std::string> Type::GetPropertyNames() const
    {
        ShouldBeClass();
        IsInit();
        /*if (typeEnum == TypeEnum::CLASS)
            return classVariable->GetPropertyNames();*/
        return instance->GetPropertyNames();
    }

    Function Type::GetMethod(const std::string& name)
    {
        ShouldBeClass();
        IsInit();
        /*if (typeEnum == TypeEnum::CLASS)
            return classVariable->GetMethod(name);*/
        return instance->GetMethod(name);
    }

    void Type::AttachInstance(const Serializable& inst) const
    {
        for (auto& [name, ofst] : instance->MembersOffset)
        {
            char* rawClassVariable = reinterpret_cast<char*>(instance.get());
            const char* rawInst = reinterpret_cast<const char*>(&inst);
            *(rawClassVariable + ofst) = *(rawInst + ofst);
        }
    }

    void Type::SetValue(const void* value) const
    {
        if (typeEnum == TypeEnum::INSTANCE || typeEnum == TypeEnum::CLASS)
        {
            AttachInstance(*reinterpret_cast<Serializable*>(const_cast<void*>(value)));
        }
        else
        {
            auto ofst = instance->MembersOffset[name];
            char* rawClassVariable = reinterpret_cast<char*>(instance.get());
            const char* rawInst = static_cast<const char*>(value);
            *(rawClassVariable + ofst) = *(rawInst + ofst);
        }
    }

    std::vector<std::string> Type::GetMethodNames() const
    {
        ShouldBeClass();
        IsInit();
        /*if (typeEnum == TypeEnum::CLASS)
            return classVariable->GetFunctionNames();*/
        return instance->GetFunctionNames();
    }

    Ref<Type> Type::GetProperty(const std::string& name)
    {
        ShouldBeClass();
        IsInit();
        /*if (classVariable)
            return classVariable->GetProperty(name);*/
        return instance->GetProperty(name);
    }

    std::string Type::GetType()
    {
        return type;
    }

    Type::TypeEnum Type::GetTypeEnum() const
    {
        return typeEnum;
    }


    void* Type::RawPtr()
    {
        /*if (classVariable)
            return classVariable.get();*/
        if (typeEnum == TypeEnum::VARIABLE)
            return reinterpret_cast<void*>(reinterpret_cast<char*>(instance.get()) + offset);
        return instance.get();
    }
}
