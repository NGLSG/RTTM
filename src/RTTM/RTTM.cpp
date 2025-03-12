#include "RTTM.hpp"

namespace RTTM
{
    RType::RType(std::string _type): type(_type)
    {
        if (detail::TypesInfo.find(type) == detail::TypesInfo.end())
        {
            throw std::runtime_error("RTTM: The structure has not been registered: " + type);
            return;
        }

        typeEnum = detail::TypesInfo[type].type;
        valid = true;
        for (const auto& [name, _] : detail::TypesInfo[type].members)
        {
            membersName.insert(name);
        }

        for (const auto& [name, _] : detail::TypesInfo[type].functions)
        {
            funcsName.insert(name);
        }
    }
}
