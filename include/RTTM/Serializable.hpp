#ifndef SERIALIZABLE_H
#define SERIALIZABLE_H

#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "Factory.hpp"
#include "Object.hpp"
#include "Function.hpp"

namespace RTTM
{
    class Serializable;
    class IEnum;
    //inline static std::unordered_set<std::shared_ptr<Serializable>> Instances; //仅用作防止指针突然崩溃
    template <typename T>
    using Ref = std::shared_ptr<T>;

    template <typename T, typename... Args>
    Ref<T> CreateRef(Args&&... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    template <typename T>
    Ref<T> CreateRef2(T* ptr)
    {
        return Ref<T>(ptr, [](T* ptr)
        {
            std::cout << "delete: " << ptr << std::endl;
            if (ptr)
            {
                delete ptr;
            }
        });
    }

    template <typename T>
    Ref<T> AliasCreate(Ref<T> manager, T* ptr)
    {
        return std::shared_ptr<T>(manager, ptr);
    }

    class Type
    {
    protected:
        enum class TypeEnum
        {
            INSTANCE,
            CLASS,
            VARIABLE,
            Enum,
        };

        friend class Serializable;

        Ref<Serializable> classVariable = nullptr;
        size_t offset;
        std::string type;
        TypeEnum typeEnum;


        void IsInit() const
        {
            if (!instance)
            {
                throw std::runtime_error("The structure has not been initialized");
            }
        }

        void ShouldBeClass() const
        {
            if (!IsClass())
            {
                throw std::runtime_error("This might be property not a structure you could not use these function");
            }
        }

    public:
        bool IsClass() const
        {
            return typeEnum == TypeEnum::CLASS || typeEnum == TypeEnum::INSTANCE;
        }

        Ref<Serializable> instance;

        Type() = default;

        void* RawPtr();

        template <typename... Args>
        void Create(Args... args);

        template <typename T, typename... Args>
        T Invoke(const std::string& name, Args... args);
        template <typename T>
        T& GetProperty(const std::string& name);
        Ref<Type> GetProperty(const std::string& name);

        std::vector<std::string> GetPropertyNames() const;

        Function GetMethod(const std::string& name);

        std::vector<std::string> GetMethodNames() const;

        Type& operator=(const Type& other) = default;

        template <typename T>
        T& As()
        {
            return *reinterpret_cast<T*>(reinterpret_cast<char*>(instance.get()) + offset);
        }

        std::string GetType();

        TypeEnum GetTypeEnum() const;
    };

    class SerializableVar
    {
    public:
        inline static std::unordered_map<std::string, Ref<Serializable>> Classes = {}; // 注册之后不可变，仅用于拷贝
        inline static std::unordered_map<std::string, Ref<IEnum>> Enums = {}; // 注册之后不可变，仅用于拷贝
        inline static std::unordered_map<std::string, std::unordered_map<std::string, Ref<IFactory>>> Factories = {};
        inline static std::unordered_map<std::string, Ref<Type>> Instances = {};
        inline static std::unordered_map<std::string, Object> Variables = {};
        inline static std::unordered_map<std::string, Ref<IFunctionWrapper>> GFunctions = {};
    };


    template <typename T>
    class Structure_;

    class IEnum
    {
    protected:;
        friend class Serializable;

    public:
        std::unordered_map<std::string, Ref<Object>> Members;

        static IEnum GetEnumByName(const std::string& name);

        template <typename T>
        static IEnum GetEnum();

        static std::vector<std::string> GetRegisteredEnum();

        template <typename T>
        IEnum& value(const char* name, T value);

        template <typename T>
        T GetValue(const std::string& name) const;
    };

    class Serializable : public std::enable_shared_from_this<Serializable>
    {
    private:
        std::unordered_map<std::string, Object> Members;
        std::unordered_map<std::string, size_t> MembersOffset;

        std::unordered_map<std::string, Ref<IFunctionWrapper>> Methods;
        friend class Type;

    public:
        virtual ~Serializable() = default;

        explicit Serializable() = default;

        Serializable& operator=(const Serializable& other);

        Function GetMethod(const std::string& name);

        Serializable DeepClone();

        std::vector<std::string> GetPropertyNames();

        std::vector<std::string> GetFunctionNames();

        Ref<Type> GetProperty(const std::string& name);

        template <typename T, typename U>
        Serializable& property(const char* name, T U::* value);

        template <typename T, typename... Args>
        Serializable& constructor();

        template <typename T>
        T& GetProperty(const std::string& name);

        template <typename R, typename... Args>
        void function(const std::string& name, std::function<R(Args...)> f);

        template <typename U, typename R, typename... Args>
        Serializable& method(const char* name, R (U::*memFunc)(Args...));

        template <typename R, typename... Args>
        void function(const std::string& name, R (*f)(Args...));

        template <typename R, typename... Args>
        R Invoke(const std::string& name, Args... args);

        template <typename T>
        static Ref<T> GetInstance(const std::string& name);

        template <typename T>
        static Ref<Type> Get();

        template <typename T>
        static Ref<Type> Get(const char* uname);

        template <typename... Args>
        static Ref<Serializable> Create(const std::string& type, Args... args);

        template <typename T>
        static void RegisterVariable(const std::string& name, const T& value);

        template <typename T>
        static Ref<T> GetVariable(const std::string& name);

        template <typename R, typename... Args>
        static void RegisterGlobalMethod(const std::string& name, R (*f)(Args...));

        template <typename R, typename... Args>
        static void RegisterGlobalMethod(const std::string& name, const std::function<R(Args...)>& func);

        template <typename F>
        static void RegisterGlobalMethod(const std::string& name, F f);

        template <typename T, typename... Args>
        static T InvokeG(const std::string& name, Args... args);

        static void Shutdown()
        {
            SerializableVar::Classes.clear();
            SerializableVar::Enums.clear();
            SerializableVar::Factories.clear();
            SerializableVar::Variables.clear();
            SerializableVar::GFunctions.clear();
        }

        static Ref<Type> GetByName(const char* type)
        {
            if (SerializableVar::Classes.find(type) == SerializableVar::Classes.end())
            {
                throw std::runtime_error(std::string("The structure has not been registered: ") + type);
            }
            auto newStruct = CreateRef<Type>();
            newStruct->instance = CreateRef<Serializable>();
            *newStruct->instance = SerializableVar::Classes[type]->DeepClone();
            newStruct->type = type;
            newStruct->typeEnum = Type::TypeEnum::INSTANCE;
            return newStruct;
        }

        static Ref<Type> GetByName(const char* uname, const char* type)
        {
            if (SerializableVar::Classes.find(type) == SerializableVar::Classes.end())
            {
                std::cout << "The structure has not been registered: " << type << std::endl;
                return nullptr;
            }
            auto newStruct = CreateRef<Type>();
            newStruct->instance = CreateRef<Serializable>();
            *newStruct->instance = SerializableVar::Classes[type]->DeepClone();
            newStruct->type = type;
            newStruct->typeEnum = Type::TypeEnum::CLASS;
            SerializableVar::Instances[uname] = newStruct;
            return newStruct;
        }

        static std::vector<std::string> GetRegisteredStructure()
        {
            std::vector<std::string> res;
            for (auto& it : SerializableVar::Classes)
            {
                res.push_back(it.first);
            }
            return res;
        }

        static Function GetGlobalMethod(const std::string& name)
        {
            auto it = SerializableVar::GFunctions.find(name);
            if (it == SerializableVar::GFunctions.end())
                throw std::runtime_error("Function not found: " + name);
            Function func(it->second, name);
            return func;
        }
    };

    template <typename T>
    class Structure_ : public Serializable
    {
        Ref<T> serialized;

    public:
        Structure_();

        template <typename... Args>
        Structure_& constructor();

        template <typename U>
        Structure_& property(const char* name, U T::* value);

        template <typename R, typename... Args>
        Structure_& method(const char* name, R (T::*memFunc)(Args...));
    };

    template <typename T>
    class Enum_ : public IEnum
    {
    private:
        Ref<IEnum> serialized;

    public:
        Enum_();

        Enum_& value(const char* name, T value);

        std::vector<std::string> GetNames() const;

        T GetValue(const std::string& name) const;
    };

    template <typename... Args>
    void Type::Create(Args... args)
    {
        ShouldBeClass();
        if (typeEnum != TypeEnum::INSTANCE && classVariable)
        {
            *classVariable = *SerializableVar::Classes[type]; // 只在操作子类时才会被赋予新的地址，防止与原始Instance脱节
            Ref<Serializable> inst = Serializable::Create(type, std::forward<Args>(args)...);
            *inst = *classVariable;

            for (const auto& [memberName, memberOffset] : classVariable->MembersOffset)
            {
                char* rawClassVariable = reinterpret_cast<char*>(classVariable.get());
                char* rawInst = reinterpret_cast<char*>(inst.get());

                *(rawClassVariable + memberOffset) = *(rawInst + memberOffset);
            }

            for (auto& [memberName, memberType] : classVariable->Members)
            {
                auto refType = memberType.As<Ref<Type>>();
                if (refType)
                {
                    refType->instance = classVariable;
                    if (refType->GetTypeEnum() == TypeEnum::CLASS)
                    {
                        auto class_ = std::dynamic_pointer_cast<Type>(refType);
                        if (class_)
                        {
                            class_->classVariable.reset();
                            class_->classVariable = CreateRef2<Serializable>(
                                reinterpret_cast<Serializable*>(reinterpret_cast<char*>(classVariable.get()) + refType->
                                    offset));
                            *class_->classVariable = SerializableVar::Classes[refType->GetType()]->DeepClone();
                            class_->Create();
                        }
                    }
                }
            }
        }
        else
        {
            instance.reset();
            auto ist = Serializable::Create(type, std::forward<Args>(args)...);
            instance = ist;
            *instance = *SerializableVar::Classes[type];

            for (auto& [memberName, memberType] : instance->Members)
            {
                auto refType = memberType.As<Ref<Type>>();
                if (refType)
                {
                    refType->instance = instance;
                    if (refType->GetTypeEnum() == TypeEnum::CLASS)
                    {
                        auto class_ = std::dynamic_pointer_cast<Type>(refType);
                        if (class_)
                        {
                            class_->classVariable.reset();
                            class_->classVariable = CreateRef2<Serializable>(
                                reinterpret_cast<Serializable*>(reinterpret_cast<char*>(instance.get()) + refType->
                                    offset));
                            *class_->classVariable = SerializableVar::Classes[refType->GetType()]->DeepClone();
                            class_->Create();
                        }
                    }
                }
            }
        }
    }

    template <typename T, typename... Args>
    T Type::Invoke(const std::string& name, Args... args)
    {
        ShouldBeClass();
        IsInit();
        if (typeEnum == TypeEnum::CLASS)
            return classVariable->Invoke<T>(name, std::forward<Args>(args)...);
        return instance->Invoke<T>(name, std::forward<Args>(args)...);
    }

    template <typename T>
    T& Type::GetProperty(const std::string& name)
    {
        ShouldBeClass();
        IsInit();
        if (!classVariable)
            return instance->GetProperty<T>(name);
        else
            return classVariable->GetProperty<T>(name);
    }

    template <typename T>
    IEnum IEnum::GetEnum()
    {
        return GetEnumByName(Object::GetTypeName<T>());
    }

    template <typename T>
    IEnum& IEnum::value(const char* name, T value)
    {
        Members[name] = CreateRef<Object>(value);
        return *this;
    }

    template <typename T>
    T IEnum::GetValue(const std::string& name) const
    {
        auto it = Members.find(name);
        if (it == Members.end())
            throw std::runtime_error("Value not found: " + name);
        auto res = it->second->As<T>();
        return res;
    }

    template <typename T, typename U>
    Serializable& Serializable::property(const char* name, T U::* value)
    {
        auto converted = static_cast<T Serializable::*>(value);
        Ref<Type> type = CreateRef<Type>();
        MembersOffset[name] = reinterpret_cast<size_t>(&(reinterpret_cast<U*>(0)->*value));

        if constexpr (std::is_base_of_v<Serializable, T>)
        {
            type->typeEnum = Type::TypeEnum::CLASS;
            type->classVariable = AliasCreate<Serializable>(shared_from_this(), &(this->*converted));
            type->classVariable->Members = SerializableVar::Classes[Object::GetTypeName<T>()]->Members;
            type->classVariable->Methods = SerializableVar::Classes[Object::GetTypeName<T>()]->Methods;
        }
        else
        {
            type->typeEnum = Type::TypeEnum::VARIABLE;
        }
        type->type = Object::GetTypeName<T>();
        type->offset = MembersOffset[name];
        Members[name] = type;

        return *this;
    }

    template <typename T, typename... Args>
    Serializable& Serializable::constructor()
    {
        if constexpr (sizeof...(Args) == 0)
        {
            auto factory = CreateRef<Factory<T>>();
            SerializableVar::Factories[Object::GetTypeName<T>()]["default"] = factory;
        }
        else
        {
            auto factory = CreateRef<Factory<T, Args...>>();
            SerializableVar::Factories[Object::GetTypeName<T>()][Object::GetTypeName<Args...>()] = factory;
        }
        return *this;
    }

    template <typename T>
    T& Serializable::GetProperty(const std::string& name)
    {
        auto it = MembersOffset.find(name);
        if (it == MembersOffset.end())
            throw std::runtime_error("Property not found: " + name);
        size_t offset = it->second;
        return *reinterpret_cast<T*>(reinterpret_cast<char*>(this) + offset);
    }

    template <typename R, typename... Args>
    void Serializable::function(const std::string& name, std::function<R(Args...)> f)
    {
        Methods[name] = CreateRef<FunctionWrapper<R, Args...>>(f);
    }

    template <typename U, typename R, typename... Args>
    Serializable& Serializable::method(const char* name, R (U::*memFunc)(Args...))
    {
        auto boundFunc = [memFunc](Serializable* instance, Args... args) -> R
        {
            auto derivedPtr = dynamic_cast<U*>(instance);
            if (derivedPtr)
            {
                return (derivedPtr->*memFunc)(std::forward<Args>(args)...);
            }
            else
            {
                throw std::runtime_error("Invalid cast to derived class");
            }
        };

        function(name, std::function<R(Serializable*, Args...)>(boundFunc));
        return *this;
    }

    template <typename R, typename... Args>
    void Serializable::function(const std::string& name, R (*f)(Args...))
    {
        function(name, std::function<R(Args...)>(f));
    }

    template <typename R, typename... Args>
    R Serializable::Invoke(const std::string& name, Args... args)
    {
        auto it = Methods.find(name);
        if (it == Methods.end())
            throw std::runtime_error("Function not found: " + name);

        auto wrapper = std::dynamic_pointer_cast<FunctionWrapper<R, Serializable*, Args...>>(it->second);
        if (!wrapper)
            throw std::runtime_error("Function signature mismatch for: " + name);
        return (*wrapper)(this, std::forward<Args>(args)...);
    }

    template <typename T>
    Ref<T> Serializable::GetInstance(const std::string& name)
    {
        if (SerializableVar::Instances.find(name) != SerializableVar::Instances.end())
        {
            return Ref<T>(
                static_cast<T*>(std::dynamic_pointer_cast<Type>(SerializableVar::Instances[name])->RawPtr()));
        }
        return nullptr;
    }

    template <typename T>
    Ref<Type> Serializable::Get()
    {
        static_assert(std::is_base_of_v<RTTM::Serializable, T>,
                      "T must be derived from Cherry::Serializable");
        std::string type = Object::GetTypeName<T>();
        if (SerializableVar::Classes.find(type) == SerializableVar::Classes.end())
        {
            std::cout << "The structure has not been registered: " << type << std::endl;
            return nullptr;
        }
        auto newStruct = CreateRef<Type>();
        newStruct->instance = CreateRef<T>();
        *newStruct->instance = SerializableVar::Classes[type]->DeepClone();
        newStruct->typeEnum = Type::TypeEnum::INSTANCE;
        newStruct->type = type;
        return newStruct;
    }

    template <typename T>
    Ref<Type> Serializable::Get(const char* uname)
    {
        static_assert(std::is_base_of_v<RTTM::Serializable, T>,
                      "T must be derived from Cherry::Serializable");
        std::string type = Object::GetTypeName<T>();
        if (SerializableVar::Classes.find(type) == SerializableVar::Classes.end())
        {
            std::cout << "The structure has not been registered: " << type << std::endl;
            return nullptr;
        }
        auto newStruct = CreateRef<Type>();
        newStruct->instance = CreateRef<T>();
        *newStruct->instance = SerializableVar::Classes[type]->DeepClone();
        newStruct->type = type;
        newStruct->typeEnum = Type::TypeEnum::INSTANCE;
        SerializableVar::Instances[uname] = newStruct;
        return newStruct;
    }

    template <typename... Args>
    Ref<Serializable> Serializable::Create(const std::string& type, Args... args)
    {
        if (SerializableVar::Factories.find(type) == SerializableVar::Factories.end())
        {
            std::cout << "The structure has not been registered: " << type << std::endl;
            return nullptr;
        }
        if constexpr (sizeof...(Args) == 0)
        {
            return SerializableVar::Factories[type]["default"]->Create();
        }
        else
            return SerializableVar::Factories[type][Object::GetTypeName<Args...>()]->Create(
                std::forward<Args>(args)...);
    }

    template <typename T>
    void Serializable::RegisterVariable(const std::string& name, const T& value)
    {
        SerializableVar::Variables[name] = Object(CreateRef<T>(value));
    }

    template <typename T>
    Ref<T> Serializable::GetVariable(const std::string& name)
    {
        auto it = SerializableVar::Variables.find(name);
        if (it == SerializableVar::Variables.end())
            throw std::runtime_error("Variable not found: " + name);
        return it->second.As<Ref<T>>();
    }

    template <typename R, typename... Args>
    void Serializable::RegisterGlobalMethod(const std::string& name, R (*f)(Args...))
    {
        SerializableVar::GFunctions[name] = CreateRef<FunctionWrapper<R, Args...>>(f);
    }

    template <typename R, typename... Args>
    void Serializable::RegisterGlobalMethod(const std::string& name, const std::function<R(Args...)>& func)
    {
        auto wrapper = CreateRef<FunctionWrapper<R, Args...>>(func);
        SerializableVar::GFunctions[name] = wrapper;
    }

    template <typename F>
    void Serializable::RegisterGlobalMethod(const std::string& name, F f)
    {
        std::function func = f;
        RegisterGlobalMethod(name, func);
    }

    template <typename T, typename... Args>
    T Serializable::InvokeG(const std::string& name, Args... args)
    {
        auto it = SerializableVar::GFunctions.find(name);
        if (it == SerializableVar::GFunctions.end())
            throw std::runtime_error("Function not found: " + name);
        return Function(it->second, name).Invoke<T>(std::forward<Args>(args)...);
    }

    template <typename T>
    Structure_<T>::Structure_()
    {
        static_assert(std::is_base_of_v<RTTM::Serializable, T>,
                      "T must be derived from Cherry::Serializable");
        static_assert(!std::is_base_of_v<RTTM::Structure_<T>, T>,
                      "T can not be derived from Cherry::Structure_<T>");
        serialized = CreateRef<T>();
        auto name = Object::GetTypeName<T>();
        SerializableVar::Classes[name] = serialized;
    }

    template <typename T>
    template <typename... Args>
    Structure_<T>& Structure_<T>::constructor()
    {
        serialized->template constructor<T, Args...>();
        return *this;
    }

    template <typename T>
    template <typename U>
    Structure_<T>& Structure_<T>::property(const char* name, U T::* value)
    {
        serialized->property(name, value);
        return *this;
    }

    template <typename T>
    template <typename R, typename... Args>
    Structure_<T>& Structure_<T>::method(const char* name, R (T::*memFunc)(Args...))
    {
        serialized->method(name, memFunc);
        return *this;
    }

    template <typename T>
    Enum_<T>::Enum_()
    {
        static_assert(std::is_enum_v<T>, "T must be an enum");
        serialized = CreateRef<IEnum>();
        SerializableVar::Enums[Object::GetTypeName<T>()] = serialized;
    }

    template <typename T>
    Enum_<T>& Enum_<T>::value(const char* name, T value)
    {
        serialized->value(name, value);
        return *this;
    }

    template <typename T>
    std::vector<std::string> Enum_<T>::GetNames() const
    {
        std::vector<std::string> names;
        for (const auto& [name, value] : serialized->Members)
        {
            names.push_back(name);
        }
        return names;
    }

    template <typename T>
    T Enum_<T>::GetValue(const std::string& name) const
    {
        return serialized->GetValue<T>(name);
    }

    using RTTIType = Ref<Type>;
}

#define CONCAT_IMPL(s1, s2) s1##s2
#define CONCAT(s1, s2) CONCAT_IMPL(s1, s2)

#define SERIALIZABLE_REGISTRATION                           \
namespace {                                               \
struct CONCAT(RegistrationHelper_, __LINE__) {          \
CONCAT(RegistrationHelper_, __LINE__)();           \
};}                                                    \
static CONCAT(RegistrationHelper_, __LINE__)           \
CONCAT(registrationHelperInstance_, __LINE__);    \
CONCAT(RegistrationHelper_, __LINE__)::CONCAT(RegistrationHelper_, __LINE__)()
#endif //SERIALIZABLE_H
