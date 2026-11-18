// SPDX-FileCopyrightText: Copyright (c) 2026 Jinmo Co., Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

// Like the rest of /App this lives in the global namespace; only the CLI
// helpers (Cli/) are nested in UsdStudio::.

// Machine-readable error categories. A frontend (CLI, MCP server, test) maps
// these to its own error codes/exit codes without parsing messages.
enum class ErrorCode
{
    InvalidValue, // token could not be parsed / wrong type
    NotFound,     // prim, attribute or file does not exist
    Precondition, // valid request, but the app state refuses it (no stage open)
    Failed,       // the operation itself was refused (USD, disk, ...)
    Internal,     // unexpected exception
};

// Thrown by commands and queries for anything a caller can act on. The CLI
// reports code, message and hint as a structured object (see Cli.h).
class CommandError : public std::runtime_error
{
public:
    CommandError(ErrorCode code, std::string message, std::string hint = {})
        : std::runtime_error(message)
        , code_(code)
        , hint_(std::move(hint))
    {
    }

    ErrorCode code() const noexcept { return code_; }
    const std::string& hint() const noexcept { return hint_; }

    static CommandError invalidValue(std::string message, std::string hint = {})
    {
        return { ErrorCode::InvalidValue, std::move(message), std::move(hint) };
    }

    static CommandError notFound(std::string message, std::string hint = {})
    {
        return { ErrorCode::NotFound, std::move(message), std::move(hint) };
    }

    static CommandError precondition(std::string message, std::string hint = {})
    {
        return { ErrorCode::Precondition, std::move(message), std::move(hint) };
    }

    static CommandError failed(std::string message, std::string hint = {})
    {
        return { ErrorCode::Failed, std::move(message), std::move(hint) };
    }

    static CommandError internal(std::string message, std::string hint = {})
    {
        return { ErrorCode::Internal, std::move(message), std::move(hint) };
    }

private:
    ErrorCode code_;
    std::string hint_;
};

inline std::string_view errorCodeName(ErrorCode code)
{
    switch (code)
    {
    case ErrorCode::InvalidValue: return "invalidValue";
    case ErrorCode::NotFound: return "notFound";
    case ErrorCode::Precondition: return "precondition";
    case ErrorCode::Failed: return "failed";
    case ErrorCode::Internal: return "internal";
    }
    return "internal";
}
