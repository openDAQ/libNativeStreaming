#include <gtest/gtest.h>
#include <native_streaming/utils/base64.hpp>

using namespace daq::native_streaming;

using Base64test = testing::Test;

TEST_F(Base64test, Hello)
{
    auto input = "Hello World!";
    auto encoded = Base64::encode(input);
    auto decoded = Base64::decode(encoded);
    ASSERT_EQ(decoded, input);
}

TEST_F(Base64test, Padding)
{
    const std::string input = "username:password";

    auto decoded1 = Base64::decode("dXNlcm5hbWU6cGFzc3dvcmQ=");
    auto decoded2 = Base64::decode("dXNlcm5hbWU6cGFzc3dvcmQ==");
    auto decoded3 = Base64::decode("dXNlcm5hbWU6cGFzc3dvcmQ===");

    ASSERT_EQ(input, decoded1);
    ASSERT_EQ(input, decoded2);
    ASSERT_EQ(input, decoded3);
}

TEST_F(Base64test, EmptyString)
{
    auto encoded = Base64::encode("");
    ASSERT_EQ("", encoded);

    auto decoded = Base64::decode("");
    ASSERT_EQ("", decoded);
}

TEST_F(Base64test, NullByte)
{
    ASSERT_EQ("", Base64::decode("="));
    ASSERT_EQ("", Base64::decode("=="));
}

TEST_F(Base64test, MoreStrings)
{
    std::string value;
    std::string encoded;
    std::string decoded;

    value = "hakuna matata";
    encoded = Base64::encode(value);
    decoded = Base64::decode(encoded);
    ASSERT_EQ(value, decoded);

    value = "banana";
    encoded = Base64::encode(value);
    decoded = Base64::decode(encoded);
    ASSERT_EQ(value, decoded);

    value = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod ...";
    encoded = Base64::encode(value);
    decoded = Base64::decode(encoded);
    ASSERT_EQ(value, decoded);

    value = "p=!;+DKvAF)Ue(w7nS;Gw}@EbMf3Yy";
    encoded = Base64::encode(value);
    decoded = Base64::decode(encoded);
    ASSERT_EQ(value, decoded);

    value = "123";
    encoded = Base64::encode(value);
    decoded = Base64::decode(encoded);
    ASSERT_EQ(value, decoded);
}
