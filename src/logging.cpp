#include <native_streaming/logging.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/logger.h>

BEGIN_NAMESPACE_NATIVE_STREAMING

LogCallback Logging::logCallback()
{
    const std::string name = "native_streaming";
    static auto logger = std::make_shared<spdlog::logger>(name, std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

    logger->set_level(spdlog::level::trace);

    return [](spdlog::source_loc location, spdlog::level::level_enum level, const char* msg) { logger->log(location, level, msg); };
}

END_NAMESPACE_NATIVE_STREAMING
