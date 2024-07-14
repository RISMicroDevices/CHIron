#pragma once

#ifndef __CHI__CHI_CONN
#define __CHI__CHI_CONN

#include <concepts>         // IWYU pragma: keep


namespace CHI {

    /*
    General Connection Configuration for all levels.
    --------------------------------
    Indicates the life cycle management of specified level of CHI bundle object.
    This is designed for zero-copy procedure implementation and performance optimization.
    CHI bundle object would be configured to reference (pointer type) if connected<level> = true.
    Otherwise, it would hold all bundle data in local instance memory.
    */
    template<bool ConnectedIO           = false,
             bool ConnectedFlit         = false,
             bool ConnectedChannel      = false,
             bool ConnectedInterface    = false,
//           bool ConnectedLink         = false,
             bool ConnectedPort         = false>
    struct Connection {
        static constexpr bool connectedIO           = ConnectedIO;
        static constexpr bool connectedFlit         = ConnectedFlit;
        static constexpr bool connectedChannel      = ConnectedChannel;
        static constexpr bool connectedInterface    = ConnectedInterface;
//      static constexpr bool connectedLink         = ConnectedLink;
        static constexpr bool connectedPort         = ConnectedPort;
    };


    /*
    IO-level Connection Configuration concept
    */
    template<class T>
    concept IOLevelConnectionConcept = requires {
        { T::connectedIO        } -> std::convertible_to<bool>;
    };

    /*
    Flit-level Connection Configuration concept
    */
    template<class T>
    concept FlitLevelConnectionConcept = requires {
        { T::connectedIO        } -> std::convertible_to<bool>;
        { T::connectedFlit      } -> std::convertible_to<bool>;
    };

    /*
    Channel-level Connection Configuration concept
    */
    template<class T>
    concept ChannelLevelConnectionConcept = requires {
        { T::connectedIO        } -> std::convertible_to<bool>;
        { T::connectedFlit      } -> std::convertible_to<bool>;
        { T::connectedChannel   } -> std::convertible_to<bool>;
    };

    /*
    Interface-level Connection Configuration concept
    */
    template<class T>
    concept InterfaceLevelConnectionConcept = requires {
        { T::connectedIO        } -> std::convertible_to<bool>;
        { T::connectedFlit      } -> std::convertible_to<bool>;
        { T::connectedChannel   } -> std::convertible_to<bool>;
        { T::connectedInterface } -> std::convertible_to<bool>;
    };

    /*
    Link-level Connection Configuration concept
    */
    template<class T>
    concept LinkLevelConnectionConcept = requires {
        { T::connectedIO        } -> std::convertible_to<bool>;
        { T::connectedFlit      } -> std::convertible_to<bool>;
        { T::connectedChannel   } -> std::convertible_to<bool>;
        { T::connectedInterface } -> std::convertible_to<bool>;
//      { T::connectedLink      } -> std::convertible_to<bool>;
    };

    /*
    Port-level Connection Configuration concept
    */
    template<class T>
    concept PortLevelConnectionConcept = requires {
        { T::connectedIO        } -> std::convertible_to<bool>;
        { T::connectedFlit      } -> std::convertible_to<bool>;
        { T::connectedChannel   } -> std::convertible_to<bool>;
        { T::connectedInterface } -> std::convertible_to<bool>;
//      { T::connectedLink      } -> std::convertible_to<bool>;
        { T::connectedPort      } -> std::convertible_to<bool>;
    };
};


namespace CHI {

    /*
    decay:
    Decay the type of a reference to the type of the object it refers to.
    If the field is configured as connected (pointer type), it would return the reference type,
    decaying the pointer type to the reference type.
    This is designed for better general Flit Object interaction.

    Example:
        decay<REQ<>::qos_t>(flit.QoS) = 0;
    */
    template<class T, class U>
    inline constexpr T& decay(U& t) noexcept
    {
        if constexpr (std::is_same_v<T, U>)
            return t;
        else
            return *t;
    }

    template<class T, class U>
    inline constexpr const T& decay(const U& t) noexcept
    {
        if constexpr (std::is_same_v<T, U>)
            return t;
        else
            return *t;
    }
}


#endif // __CHI__CHI_CONN
