#include <native_streaming/async_reader.hpp>
#include <native_streaming/async_writer.hpp>
#include <gtest/gtest.h>

using namespace daq::native_streaming;

using TaskTest = testing::Test;

TEST_F(TaskTest, WriteTaskCreate)
{
    auto handler = []() {};
    size_t payload = 0;
    boost::asio::const_buffer buffer(&payload, sizeof(payload));
    WriteTask task(buffer, handler);

    ASSERT_NE(task.getHandler(), nullptr);
    ASSERT_EQ(task.getBuffer().data(), buffer.data());
    ASSERT_EQ(task.getBuffer().size(), buffer.size());

    ASSERT_DEATH_IF_SUPPORTED({ WriteTask(boost::asio::const_buffer(), handler); }, "");
    ASSERT_DEATH_IF_SUPPORTED({ WriteTask(buffer, nullptr); }, "");
}

TEST_F(TaskTest, ReadTaskCreateDefault)
{
    ReadTask task;
    ASSERT_EQ(task.getHandler(), nullptr);
    ASSERT_EQ(task.getSize(), 0);
}

TEST_F(TaskTest, ReadTaskCreate)
{
    auto handler = [](const void*, size_t) { return ReadTask(); };
    ReadTask task(handler, 1);

    ASSERT_NE(task.getHandler(), nullptr);
    ASSERT_EQ(task.getSize(), 1);

    ASSERT_DEATH_IF_SUPPORTED({ ReadTask(nullptr, 1); }, "");
    ASSERT_DEATH_IF_SUPPORTED({ ReadTask(handler, 0); }, "");
}
