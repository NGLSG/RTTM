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

    void Serializable::SetMembers(const std::unordered_map<std::string, Ref<Type>>& members)
    {
        Members.insert(members.begin(), members.end());
    }

    void Serializable::SetMethods(const std::unordered_map<std::string, Ref<IFunctionWrapper>>& methods)
    {
        Methods.insert(methods.begin(), methods.end());
    }

    void Serializable::SetMembersOffset(const std::unordered_map<std::string, size_t>& offset)
    {
        MembersOffset.insert(offset.begin(), offset.end());
    }

    void Serializable::SetMembersName(const std::vector<std::string>& name)
    {
        MembersName.insert(MembersName.end(), name.begin(), name.end());
    }

    void Serializable::SetMethodsName(const std::vector<std::string>& name)
    {
        MethodsName.insert(MethodsName.end(), name.begin(), name.end());
    }

    void Serializable::SetMembersType(const std::vector<Ref<Type>>& type)
    {
        MembersType.insert(MembersType.end(), type.begin(), type.end());
    }

    std::unordered_map<std::string, Ref<Type>>& Serializable::GetMembers()
    {
        return Members;
    }

    std::unordered_map<std::string, size_t>& Serializable::GetMembersOffset()
    {
        return MembersOffset;
    }

    std::unordered_map<std::string, Ref<IFunctionWrapper>>& Serializable::GetMethods()
    {
        return Methods;
    }

    std::vector<std::string>& Serializable::GetMembersName()
    {
        return MembersName;
    }

    std::vector<std::string>& Serializable::GetMethodsName()
    {
        return MethodsName;
    }

    std::vector<Ref<Type>>& Serializable::GetMembersType()
    {
        return MembersType;
    }

    Serializable& Serializable::operator=(const Serializable& other)
    {
        Members = other.Members;
        Methods = other.Methods;
        MembersOffset = other.MembersOffset;
        MembersName = other.MembersName;
        MethodsName = other.MethodsName;
        MembersType = other.MembersType;
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

    std::vector<std::string> Serializable::GetPropertyNames() const
    {
        return MembersName;
    }

    std::vector<std::string> Serializable::GetFunctionNames() const
    {
        return MethodsName;
    }

    const std::vector<Ref<Type>>& Serializable::GetProperties() noexcept
    {
        return MembersType;
    }

    Ref<Type> Serializable::GetProperty(const std::string& name)
    {
        auto it = Members.find(name);
        if (it == Members.end())
            throw std::runtime_error("Property not found: " + name);
        return it->second;
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

    void Type::AttachInstance(Serializable* inst) const
    {
        /*for (auto& [name, ofst] : instance->MembersOffset)
        {
            char* rawClassVariable = reinterpret_cast<char*>(instance.get());
            char* rawInst = reinterpret_cast<char*>(inst);
            std::cout << *(rawInst + ofst) << std::endl;
            *(rawClassVariable + ofst) = *(rawInst + ofst);
        }*/
        inst->MembersOffset = instance->MembersOffset;
        inst->Members = instance->Members;
        inst->Methods = instance->Methods;
        memcpy(instance.get(), inst, size);
    }

    std::vector<std::string> Type::GetMethodNames() const
    {
        ShouldBeClass();
        IsInit();
        /*if (typeEnum == TypeEnum::CLASS)
            return classVariable->GetFunctionNames();*/
        return instance->GetFunctionNames();
    }

    Ref<Type> Type::GetProperty(const std::string& name) const
    {
        ShouldBeClass();
        IsInit();
        /*if (classVariable)
            return classVariable->GetProperty(name);*/
        return instance->GetProperty(name);
    }

    const std::vector<Ref<Type>>& Type::GetProperties() const noexcept
    {
        return instance->GetProperties();
    }

    std::string Type::GetType()
    {
        return type;
    }

    Type::TypeEnum Type::GetTypeEnum() const
    {
        return typeEnum;
    }


    bool Type::IsClass() const
    {
        return typeEnum == TypeEnum::CLASS || typeEnum == TypeEnum::INSTANCE;
    }

    const std::string& Type::GetName() const
    {
        return name;
    }

    void* Type::RawPtr() const
    {
        /*if (classVariable)
            return classVariable.get();*/
        if (typeEnum == TypeEnum::VARIABLE)
            return reinterpret_cast<void*>(reinterpret_cast<char*>(instance.get()) + offset);
        return instance.get();
    }
}
