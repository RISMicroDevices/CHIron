#pragma once

#include "common.hpp"

#include <iostream>
#include <vector>


#ifdef CHI_ISSUE_E
#   include "../../../chi_eb/xact/chi_eb_xact_state.hpp"        // IWYU pragma: keep
#   include "../../../chi_eb/xact/chi_eb_joint.hpp"             // IWYU pragma: keep
#endif


template<class T>
class TCPt {
protected:
    Xact::Global<config>                    glbl;

protected:
    size_t* totalCount          = nullptr;
    size_t* errCountFail        = nullptr;
    size_t* errCountEnvError    = nullptr;

    bool    envError            = false;

    std::vector<std::string>*   errList;

protected:
    T*      prev                = nullptr;

public:
    virtual ~TCPt() noexcept = default;

    TCPt() noexcept
    {
        glbl.CHECK_FIELD_MAPPING->enable = false;
    }

    inline T* TotalCount(size_t* totalCount) noexcept
    {
        this->totalCount = totalCount;
        return static_cast<T*>(this);
    }

    inline T* ErrCountFail(size_t* errCountFail) noexcept
    {
        this->errCountFail = errCountFail;
        return static_cast<T*>(this);
    }

    inline T* ErrCountEnvError(size_t* errCountEnvError) noexcept
    {
        this->errCountEnvError = errCountEnvError;
        return static_cast<T*>(this);
    }

    inline T* ErrList(std::vector<std::string>* errList) noexcept
    {
        this->errList = errList;
        return static_cast<T*>(this);
    }

    inline T* Topology(Xact::Topology topo) noexcept
    {
        *this->glbl.TOPOLOGY = topo;
        return static_cast<T*>(this);
    }

    inline T* Begin() noexcept
    {
        std::cout << "\n[====] ================================================" << std::endl;
        return static_cast<T*>(this);
    }

    inline T* Rest() noexcept
    {
        T* prev = static_cast<T*>(this->prev);
        delete this;
        return prev;
    }

    inline void End() noexcept
    {
        Rest();
        std::cout << std::endl;
    }
};
