#pragma once

#include <functional>
#include <string>
#include <git2/credential.h>

#include "fsystem.hpp"

namespace Toolbox::Git {

    /*
    This function is used as a callback by libgit2 to provide credentials
    */
    int GitCredentialsProvider(git_credential **out, const char *url, const char *username_from_url,
                               unsigned int allowed_types, void *payload);

    void GitConfigureCredentials(const std::string &username, const std::string &password);
    void GitConfigureCredentials(const std::string &username, const std::string &password,
                                 const std::vector<char> &public_key,
                                 const std::vector<char> &private_key);

}  // namespace Toolbox::Git