#pragma once

#include "json.hpp"
#include <expected>
#include <functional>
#include <stacktrace>
#include <string>

using json = nlohmann::json;

struct JSONError {
    std::string m_message;
    std::string m_reason;
    std::size_t m_byte;
    std::stacktrace m_trace;
};

// Pass a callback function that operates on a json object and potentially returns a value.
template <class _Func>
inline auto tryJSON(json &j, _Func json_op)
    -> std::enable_if_t<!std::is_same_v<decltype(json_op(j)), void>,
                        std::expected<decltype(json_op(j)), JSONError>> {
    try {
        return json_op(j);
    } catch (json::type_error &err) {
        JSONError json_err = {"Error while parsing template JSON.", err.what(), 0,
                              std::stacktrace()};
        return std::unexpected(json_err);
    } catch (json::parse_error &err) {
        JSONError json_err = {"Error while parsing template JSON.", err.what(), err.byte,
                              std::stacktrace()};
        return std::unexpected(json_err);
    } catch (json::out_of_range &err) {
        JSONError json_err = {"Error while parsing template JSON.", err.what(), 0,
                              std::stacktrace()};
        return std::unexpected(json_err);
    } catch (json::invalid_iterator &err) {
        JSONError json_err = {"Error while parsing template JSON.", err.what(), 0,
                              std::stacktrace()};
        return std::unexpected(json_err);
    } catch (json::other_error &err) {
        JSONError json_err = {"Error while parsing template JSON.", err.what(), 0,
                              std::stacktrace()};
        return std::unexpected(json_err);
    }
}

// Pass a callback function that operates on a json object and potentially returns a value.
template <class _Func>
inline auto tryJSON(json &j, _Func json_op)
    -> std::enable_if_t<std::is_same_v<decltype(json_op(j)), void>,
                        std::expected<void, JSONError>> {
    try {
        json_op(j);
        return {};
    } catch (json::type_error &err) {
        JSONError json_err = {"Error while parsing template JSON.", err.what(), 0,
                              std::stacktrace()};
        return std::unexpected(json_err);
    } catch (json::parse_error &err) {
        JSONError json_err = {"Error while parsing template JSON.", err.what(), err.byte,
                              std::stacktrace()};
        return std::unexpected(json_err);
    } catch (json::out_of_range &err) {
        JSONError json_err = {"Error while parsing template JSON.", err.what(), 0,
                              std::stacktrace()};
        return std::unexpected(json_err);
    } catch (json::invalid_iterator &err) {
        JSONError json_err = {"Error while parsing template JSON.", err.what(), 0,
                              std::stacktrace()};
        return std::unexpected(json_err);
    } catch (json::other_error &err) {
        JSONError json_err = {"Error while parsing template JSON.", err.what(), 0,
                              std::stacktrace()};
        return std::unexpected(json_err);
    }
}