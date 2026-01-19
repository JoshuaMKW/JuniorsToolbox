#pragma once

#include <git2.h>
#include <git2/credential.h>

#include "fsystem.hpp"
#include "core/log.hpp"
#include "github/credentials.hpp"

namespace Toolbox::Git {

    static std::string s_git_username          = "";
    static std::string s_git_password          = "";
    static Toolbox::fs_path s_public_key_path  = "";
    static Toolbox::fs_path s_private_key_path = "";

    static int s_git_cred_types       = 0;

    int GitCredentialsProvider(git_credential **out, const char *url,
                                const char *username_from_url, unsigned int allowed_types,
                                void *payload) {
        if (s_git_cred_types & allowed_types) {
            if (s_git_cred_types == GIT_CREDENTIAL_USERPASS_PLAINTEXT) {
                if (git_credential_userpass_plaintext_new(out, s_git_username.c_str(),
                                                          s_git_password.c_str()) != 0) {
                    TOOLBOX_ERROR_V("Failed to create Git plaintext credential for URL {}", url);
                    return GIT_ERROR;
                }
                return GIT_OK;
            } else if (s_git_cred_types == GIT_CREDENTIAL_SSH_KEY) {
                if (git_credential_ssh_key_new(out, s_git_username.c_str(), nullptr, nullptr,
                                               s_git_password.c_str()) != 0) {
                    TOOLBOX_ERROR_V("Failed to create Git SSH key credential for URL {}", url);
                    return GIT_ERROR;
                }
                return GIT_OK;
            }
        }
        return GIT_PASSTHROUGH;
    }

    void GitConfigureCredentials(const std::string &username, const std::string &password) {
        s_git_username   = username;
        s_git_password   = password;
        s_git_cred_types = GIT_CREDENTIAL_USERPASS_PLAINTEXT;
    }

    void GitConfigureCredentials(const std::string &username, const std::string &password,
                                 const std::vector<char> &public_key,
                                 const std::vector<char> &private_key) {
        s_git_username   = username;
        s_git_password   = password;
        s_git_cred_types = GIT_CREDENTIAL_SSH_KEY;
    }

}  // namespace Toolbox::Git