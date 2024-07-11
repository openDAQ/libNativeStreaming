#include <native_streaming/authentication.hpp>
#include <native_streaming/utils/base64.hpp>
#include <boost/algorithm/string.hpp>

BEGIN_NAMESPACE_NATIVE_STREAMING

Authentication::Authentication()
    : type(AuthenticationType::ANONYMOUS)
{
}

Authentication::Authentication(const std::string& username, const std::string& password)
    : type(AuthenticationType::BASIC)
    , username(username)
    , password(password)
{
}

Authentication Authentication::fromHeader(const std::string& header)
{
    std::vector<std::string> headerParts;
    boost::split(headerParts, header, boost::is_space(), boost::token_compress_on);

    if (headerParts.size() >= 2)
    {
        const auto authType = headerParts[0];
        const auto value = Base64::decode(headerParts[1]);

        if (authType == "Basic")
        {
            std::vector<std::string> authParts;
            boost::split(authParts, value, boost::is_any_of(":"), boost::token_compress_on);

            if (authParts.size() < 2)
                throw std::runtime_error("Invalid basic authorization header");

            return Authentication(authParts[0], authParts[1]);
        }
    }

    throw std::runtime_error("Invalid authorization header");
}

AuthenticationType Authentication::getType() const
{
    return type;
}

std::string Authentication::getUsername() const
{
    return username;
}

std::string Authentication::getPassword() const
{
    return password;
}

std::string Authentication::getEncodedHeader() const
{
    switch (type)
    {
        case daq::native_streaming::AuthenticationType::BASIC:
            return "Basic " + Base64::encode(username + ":" + password);
    }

    return "";
}

END_NAMESPACE_NATIVE_STREAMING
