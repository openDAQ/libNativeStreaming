#include <native_streaming/utils/base64.hpp>
#include <boost/beast/core/detail/base64.hpp>

BEGIN_NAMESPACE_NATIVE_STREAMING

std::string Base64::encode(const std::string& input)
{
    const size_t maxSize = boost::beast::detail::base64::encoded_size(input.size());

    std::string buffer;
    buffer.resize(maxSize);

    const size_t size = boost::beast::detail::base64::encode(buffer.data(), input.c_str(), input.size());
    buffer.resize(size);

    return buffer;
}

std::string Base64::decode(const std::string& input)
{
    const size_t maxSize = boost::beast::detail::base64::decoded_size(input.size());

    std::string buffer;
    buffer.resize(maxSize);

    const auto [size, _] = boost::beast::detail::base64::decode(buffer.data(), input.c_str(), input.size());
    buffer.resize(size);

    return buffer;
}

END_NAMESPACE_NATIVE_STREAMING
