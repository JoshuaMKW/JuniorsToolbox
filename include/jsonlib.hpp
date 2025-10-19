#pragma once

#include <expected>
#include <functional>
#include <stacktrace>
#include <string>

#include "core/error.hpp"
#include "json.hpp"

using json = nlohmann::json;

struct JSONError : public Toolbox::BaseError {
    std::string m_reason;
    std::size_t m_byte;
};

template <typename T>
inline Toolbox::Result<T, JSONError> make_json_error(const std::string &context,
                                                     const std::string &reason, std::size_t byte) {
    JSONError json_err = {std::vector<std::string>({context}), std::stacktrace(), reason, byte};
    return std::unexpected(json_err);
}

// Pass a callback function that operates on a json object and potentially returns a value.
template <class _JsonT, class _Func>
inline auto tryJSON(_JsonT &j, _Func json_op)
    -> std::enable_if_t<!std::is_same_v<decltype(json_op(j)), void>,
                        Toolbox::Result<decltype(json_op(j)), JSONError>> {
    try {
        return json_op(j);
    } catch (json::type_error &err) {
        return make_json_error<decltype(json_op(j))>("Error while parsing template JSON.",
                                                     err.what(), 0);
    } catch (json::parse_error &err) {
        return make_json_error<decltype(json_op(j))>("Error while parsing template JSON.",
                                                     err.what(), err.byte);
    } catch (json::out_of_range &err) {
        return make_json_error<decltype(json_op(j))>("Error while parsing template JSON.",
                                                     err.what(), 0);
    } catch (json::invalid_iterator &err) {
        return make_json_error<decltype(json_op(j))>("Error while parsing template JSON.",
                                                     err.what(), 0);
    } catch (json::other_error &err) {
        return make_json_error<decltype(json_op(j))>("Error while parsing template JSON.",
                                                     err.what(), 0);
    }
}

// Pass a callback function that operates on a json object and potentially returns a value.
template <class _JsonT, class _Func>
inline auto tryJSON(_JsonT &j, _Func json_op)
    -> std::enable_if_t<std::is_same_v<decltype(json_op(j)), void>,
                        Toolbox::Result<void, JSONError>> {
    try {
        json_op(j);
        return {};
    } catch (json::type_error &err) {
        return make_json_error<void>("Error while parsing template JSON.", err.what(), 0);
    } catch (json::parse_error &err) {
        return make_json_error<void>("Error while parsing template JSON.", err.what(), err.byte);
    } catch (json::out_of_range &err) {
        return make_json_error<void>("Error while parsing template JSON.", err.what(), 0);
    } catch (json::invalid_iterator &err) {
        return make_json_error<void>("Error while parsing template JSON.", err.what(), 0);
    } catch (json::other_error &err) {
        return make_json_error<void>("Error while parsing template JSON.", err.what(), 0);
    }
}

// Pass a callback function that operates on a json object and potentially returns a value.
template <class _JsonT, class _Func>
inline auto tryJSON(const _JsonT &j, _Func json_op)
    -> std::enable_if_t<!std::is_same_v<decltype(json_op(j)), void>,
                        Toolbox::Result<decltype(json_op(j)), JSONError>> {
    try {
        return json_op(j);
    } catch (json::type_error &err) {
        return make_json_error<decltype(json_op(j))>("Error while parsing template JSON.",
                                                     err.what(), 0);
    } catch (json::parse_error &err) {
        return make_json_error<decltype(json_op(j))>("Error while parsing template JSON.",
                                                     err.what(), err.byte);
    } catch (json::out_of_range &err) {
        return make_json_error<decltype(json_op(j))>("Error while parsing template JSON.",
                                                     err.what(), 0);
    } catch (json::invalid_iterator &err) {
        return make_json_error<decltype(json_op(j))>("Error while parsing template JSON.",
                                                     err.what(), 0);
    } catch (json::other_error &err) {
        return make_json_error<decltype(json_op(j))>("Error while parsing template JSON.",
                                                     err.what(), 0);
    }
}

// Pass a callback function that operates on a json object and potentially returns a value.
template <class _JsonT, class _Func>
inline auto tryJSON(const _JsonT &j, _Func json_op)
    -> std::enable_if_t<std::is_same_v<decltype(json_op(j)), void>,
                        Toolbox::Result<void, JSONError>> {
    try {
        json_op(j);
        return {};
    } catch (json::type_error &err) {
        return make_json_error<void>("Error while parsing template JSON.", err.what(), 0);
    } catch (json::parse_error &err) {
        return make_json_error<void>("Error while parsing template JSON.", err.what(), err.byte);
    } catch (json::out_of_range &err) {
        return make_json_error<void>("Error while parsing template JSON.", err.what(), 0);
    } catch (json::invalid_iterator &err) {
        return make_json_error<void>("Error while parsing template JSON.", err.what(), 0);
    } catch (json::other_error &err) {
        return make_json_error<void>("Error while parsing template JSON.", err.what(), 0);
    }
}

// Pass a callback function that operates on a json object and potentially returns a value.
template <class _JsonT, class _Func>
inline auto tryJSONWithResult(_JsonT &j, _Func json_op) -> Toolbox::Result<void, JSONError> {
    try {
        return json_op(j);
    } catch (json::type_error &err) {
        return make_json_error<void>("Error while parsing template JSON.", err.what(), 0);
    } catch (json::parse_error &err) {
        return make_json_error<void>("Error while parsing template JSON.", err.what(), err.byte);
    } catch (json::out_of_range &err) {
        return make_json_error<void>("Error while parsing template JSON.", err.what(), 0);
    } catch (json::invalid_iterator &err) {
        return make_json_error<void>("Error while parsing template JSON.", err.what(), 0);
    } catch (json::other_error &err) {
        return make_json_error<void>("Error while parsing template JSON.", err.what(), 0);
    }
}

// Pass a callback function that operates on a json object and potentially returns a value.
template <class _JsonT, class _Func>
inline auto tryJSONWithResult(const _JsonT &j, _Func json_op) -> Toolbox::Result<void, JSONError> {
    try {
        return json_op(j);
    } catch (json::type_error &err) {
        return make_json_error<void>("Error while parsing template JSON.", err.what(), 0);
    } catch (json::parse_error &err) {
        return make_json_error<void>("Error while parsing template JSON.", err.what(), err.byte);
    } catch (json::out_of_range &err) {
        return make_json_error<void>("Error while parsing template JSON.", err.what(), 0);
    } catch (json::invalid_iterator &err) {
        return make_json_error<void>("Error while parsing template JSON.", err.what(), 0);
    } catch (json::other_error &err) {
        return make_json_error<void>("Error while parsing template JSON.", err.what(), 0);
    }
}