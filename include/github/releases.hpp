#pragma once

#include <string>

#include <netpp/client.h>

#include "core/memory.hpp"

namespace Toolbox {

    class GitReleasesClient {
    public:
        GitReleasesClient();
        ~GitReleasesClient();

        bool connect();
        void disconnect();

    private:
        ScopePtr<netpp::TCP_Client> m_client;
    };

}  // namespace Toolbox