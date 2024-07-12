#include <gtest/gtest.h>
#include <native_streaming/authentication.hpp>

using namespace daq::native_streaming;

using AuthenticationObjectTest = testing::Test;

TEST_F(AuthenticationObjectTest, Default)
{
    Authentication authentication;

    ASSERT_EQ(authentication.getType(), AuthenticationType::Anonymous);
    ASSERT_EQ(authentication.getUsername(), "");
    ASSERT_EQ(authentication.getPassword(), "");
    ASSERT_EQ(authentication.getEncodedHeader(), "");
}

TEST_F(AuthenticationObjectTest, Basic)
{
    Authentication authentication("jure", "jure123");

    ASSERT_EQ(authentication.getType(), AuthenticationType::Basic);
    ASSERT_EQ(authentication.getUsername(), "jure");
    ASSERT_EQ(authentication.getPassword(), "jure123");
    ASSERT_EQ(authentication.getEncodedHeader(), "Basic anVyZTpqdXJlMTIz");
}

TEST_F(AuthenticationObjectTest, DecodeEmpty)
{
    ASSERT_ANY_THROW(Authentication::fromHeader(""));
}

TEST_F(AuthenticationObjectTest, DecodeUsernamePasword)
{
    const std::string header = "Basic dXNlcm5hbWU6cGFzc3dvcmQ=";

    Authentication authentication = Authentication::fromHeader(header);

    ASSERT_EQ(authentication.getType(), AuthenticationType::Basic);
    ASSERT_EQ(authentication.getUsername(), "username");
    ASSERT_EQ(authentication.getPassword(), "password");
    ASSERT_EQ(authentication.getEncodedHeader(), header);
}

TEST_F(AuthenticationObjectTest, DecodeUsernamePasswordEmpty)
{
    const std::string header = "Basic Og==";

    Authentication authentication = Authentication::fromHeader(header);

    ASSERT_EQ(authentication.getType(), AuthenticationType::Basic);
    ASSERT_EQ(authentication.getUsername(), "");
    ASSERT_EQ(authentication.getPassword(), "");
    ASSERT_EQ(authentication.getEncodedHeader(), header);
}

TEST_F(AuthenticationObjectTest, DecodeInvalid)
{
    ASSERT_ANY_THROW(Authentication::fromHeader("Basic dXNlcm5hbWU="));
    ASSERT_ANY_THROW(Authentication::fromHeader("Basic ####"));
}
