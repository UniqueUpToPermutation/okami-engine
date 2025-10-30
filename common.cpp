#include "common.hpp"

#include <sstream>

using namespace okami;

Error& Error::Union(Error const& other) {
    if (IsOk()) {
        // If this is ok, just take the other
        m_contents = other.m_contents;
    }
    else if (!other.IsOk()) {
        // If both are errors, combine them
        if (std::holds_alternative<std::vector<Error>>(m_contents)) {
            auto& vec = std::get<std::vector<Error>>(m_contents);
            if (std::holds_alternative<std::vector<Error>>(other.m_contents)) {
                const auto& otherVec = std::get<std::vector<Error>>(other.m_contents);
                vec.insert(vec.end(), otherVec.begin(), otherVec.end());
            }
            else {
                vec.push_back(other);
            }
        }
        else {
            // Convert this to a vector
            std::vector<Error> vec;
            vec.push_back(*this);
            if (std::holds_alternative<std::vector<Error>>(other.m_contents)) {
                const auto& otherVec = std::get<std::vector<Error>>(other.m_contents);
                vec.insert(vec.end(), otherVec.begin(), otherVec.end());
            }
            else {
                vec.push_back(other);
            }
            m_contents = std::move(vec);
        }
    }
    return *this;
}

std::string Error::Str() const {
    std::stringstream ss;
    if (std::holds_alternative<std::string_view>(m_contents)) {
        return std::string(std::get<std::string_view>(m_contents));
    }
    else if (std::holds_alternative<std::string>(m_contents)) {
        return std::get<std::string>(m_contents);
    } else if (std::holds_alternative<std::vector<Error>>(m_contents)) {
        const auto& vec = std::get<std::vector<Error>>(m_contents);
        for (const auto& e : vec) {
            if (!ss.str().empty()) {
                ss << "; ";
            }
            ss << e.Str();
        }
        return ss.str();
    }
    else {
        return "No error";
    }
}