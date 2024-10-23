#include <native_streaming/server.hpp>
#include <native_streaming/client.hpp>
#include <gtest/gtest.h>
#include <native_streaming/logging.hpp>

#include "test_connection.hpp"

#include <future>

using namespace daq::native_streaming;

class MessagesTest : public ConnectionTest
{
public:
    MessagesTest()
        : ConnectionTest()
    {
        onRWErrorCallback = [&](const std::string& message, std::shared_ptr<Session>)
        { ADD_FAILURE() << "R/W operation failed: " << message; };
    }

protected:
    void ResetFuturePromise()
    {
        serverReadPromise = std::promise<void>();
        serverReadFuture = serverReadPromise.get_future();
        serverWritePromise = std::promise<void>();
        serverWriteFuture = serverWritePromise.get_future();
        clientReadPromise = std::promise<void>();
        clientReadFuture = clientReadPromise.get_future();
        clientWritePromise = std::promise<void>();
        clientWriteFuture = clientWritePromise.get_future();
    }
    void SetUp() override
    {
        ConnectionTest::SetUp();

        ResetFuturePromise();

        server = std::make_shared<Server>(onNewServerSessionCallback, onAuthenticateCallback, ioContextPtrServer, logCallback);
        server->start(CONNECTION_PORT);
        client = std::make_shared<Client>(CONNECTION_HOST,
                                          std::to_string(CONNECTION_PORT),
                                          CONNECTION_PATH,
                                          authentication,
                                          onNewClientSessionCallback,
                                          onResolveFailedCallback,
                                          onConnectFailedCallback,
                                          onHandshakeFailedCallback,
                                          ioContextPtrClient,
                                          logCallback);
        client->connect();

        clientConnectedFuture.wait_for(timeout);
        serverConnectedFuture.wait_for(timeout);

        serverSession->setErrorHandlers(onRWErrorCallback, onRWErrorCallback);
        clientSession->setErrorHandlers(onRWErrorCallback, onRWErrorCallback);
    }

    void TearDown() override
    {
        clientSession->close(onClientSessionClosedCallback);
        serverSession->close(onServerSessionClosedCallback);
        clientDisconnectedFuture.wait_for(timeout);
        serverDisconnectedFuture.wait_for(timeout);

        server.reset();
        client.reset();
        ConnectionTest::TearDown();
    }

    std::promise<void> serverReadPromise;
    std::future<void> serverReadFuture;

    std::promise<void> serverWritePromise;
    std::future<void> serverWriteFuture;

    std::promise<void> clientReadPromise;
    std::future<void> clientReadFuture;

    std::promise<void> clientWritePromise;
    std::future<void> clientWriteFuture;

    std::shared_ptr<Server> server;
    std::shared_ptr<Client> client;

    OnSessionErrorCallback onRWErrorCallback;
};

// Sender sends multiple strings, reader reads as a single string
TEST_F(MessagesTest, MessagesFromServerToClient)
{
    std::vector<std::string> sendStrings{"one", "two", "three", "four"};
    std::string received;
    auto onClientReadCallback = [&, this](const void* data, size_t size)
    {
        received = std::string(reinterpret_cast<const char*>(data), size);
        clientReadPromise.set_value();
        return ReadTask();  // finish read sequence
    };

    std::stringstream sent;
    std::vector<WriteTask> tasks;
    for (size_t i = 0; i < sendStrings.size(); ++i)
    {
        auto payloadString = std::make_shared<std::string>(sendStrings[i]);
        auto writeHandler = [payloadString]() {};
        boost::asio::const_buffer buffer(payloadString->data(), payloadString->size());
        tasks.push_back(WriteTask(buffer, writeHandler));
        sent << *payloadString;
    }
    serverSession->scheduleWrite(std::move(tasks));
    clientSession->scheduleRead(ReadTask(onClientReadCallback, sent.str().size()));

    EXPECT_EQ(clientReadFuture.wait_for(timeout), std::future_status::ready);

    EXPECT_EQ(sent.str(), received);
}

// Sender sends multiple strings, reader reads as a single string
TEST_F(MessagesTest, MessagesFromClientToServer)
{
    std::vector<std::string> sendStrings{"one", "two", "three", "four"};
    std::string received;
    auto onServerReadCallback = [&, this](const void* data, size_t size)
    {
        received = std::string(reinterpret_cast<const char*>(data), size);
        serverReadPromise.set_value();
        return ReadTask();  // finish read sequence
    };

    std::stringstream sent;
    std::vector<WriteTask> tasks;
    for (size_t i = 0; i < sendStrings.size(); ++i)
    {
        auto payloadString = std::make_shared<std::string>(sendStrings[i]);
        auto writeHandler = [payloadString]() {};
        boost::asio::const_buffer buffer(payloadString->data(), payloadString->size());
        tasks.push_back(WriteTask(buffer, writeHandler));
        sent << *payloadString;
    }
    clientSession->scheduleWrite(std::move(tasks));
    serverSession->scheduleRead(ReadTask(onServerReadCallback, sent.str().size()));

    EXPECT_EQ(serverReadFuture.wait_for(timeout), std::future_status::ready);

    EXPECT_EQ(sent.str(), received);
}

// Sender sends multiple strings, reader reads as a single string
TEST_F(MessagesTest, MessagesBidirectional)
{
    std::string clientReceived;
    auto onClientReadCallback = [&, this](const void* data, size_t size)
    {
        clientReceived = std::string(reinterpret_cast<const char*>(data), size);
        clientReadPromise.set_value();
        return ReadTask();  // finish read sequence
    };

    std::string serverReceived;
    auto onServerReadCallback = [&, this](const void* data, size_t size)
    {
        serverReceived = std::string(reinterpret_cast<const char*>(data), size);
        serverReadPromise.set_value();
        return ReadTask();  // finish read sequence
    };

    std::stringstream serverSent;
    {
        std::vector<std::string> sendStrings{"from-srv-one", "from-srv-two", "from-srv-three", "from-srv-four"};
        std::vector<WriteTask> tasks;
        for (size_t i = 0; i < sendStrings.size(); ++i)
        {
            auto payloadString = std::make_shared<std::string>(sendStrings[i]);
            auto writeHandler = [payloadString]() {};
            boost::asio::const_buffer buffer(payloadString->data(), payloadString->size());
            tasks.push_back(WriteTask(buffer, writeHandler));
            serverSent << *payloadString;
        }
        serverSession->scheduleWrite(std::move(tasks));
    }
    std::stringstream clientSent;
    {
        std::vector<std::string> sendStrings{"from-cl-one", "from-cl-two", "from-cl-three", "from-cl-four"};
        std::vector<WriteTask> tasks;
        for (size_t i = 0; i < sendStrings.size(); ++i)
        {
            auto payloadString = std::make_shared<std::string>(sendStrings[i]);
            auto writeHandler = [payloadString]() {};
            boost::asio::const_buffer buffer(payloadString->data(), payloadString->size());
            tasks.push_back(WriteTask(buffer, writeHandler));
            clientSent << *payloadString;
        }
        clientSession->scheduleWrite(std::move(tasks));
    }

    serverSession->scheduleRead(ReadTask(onServerReadCallback, clientSent.str().size()));
    clientSession->scheduleRead(ReadTask(onClientReadCallback, serverSent.str().size()));

    EXPECT_EQ(serverReadFuture.wait_for(timeout), std::future_status::ready);
    EXPECT_EQ(clientReadFuture.wait_for(timeout), std::future_status::ready);

    EXPECT_EQ(clientSent.str(), serverReceived);
    EXPECT_EQ(serverSent.str(), clientReceived);
}

// Sender sends multiple strings, reader reads multiple strings as messages with header and payload
// data is written from single separate thread
TEST_F(MessagesTest, SplitMessages)
{
    auto writerSession = serverSession;
    auto readerSession = clientSession;

    std::promise<void> writerPromise;
    std::future<void> writerFuture = writerPromise.get_future();

    std::promise<void> readerPromise;
    std::future<void> readerFuture = readerPromise.get_future();

    std::vector<std::string> toSend{"one", "two", "three", "four"};
    std::vector<std::string> received;

    size_t sendCounter = 0;
    size_t recvCounter = 0;

    ReadHandler onReadPayload;
    ReadHandler onReadHeader = [&](const void* data, size_t size) -> ReadTask
    {
        size_t nextPayloadSize;
        memcpy(&nextPayloadSize, data, sizeof(nextPayloadSize));
        NS_LOG_T("Read header: size {}, value {}", sizeof(nextPayloadSize), nextPayloadSize);
        return ReadTask(onReadPayload, nextPayloadSize);
    };
    onReadPayload = [&](const void* data, size_t size) -> ReadTask
    {
        auto payload = std::string(reinterpret_cast<const char*>(data), size);
        received.push_back(payload);

        NS_LOG_T("Read payload: {}", payload);
        if (++recvCounter == toSend.size())
        {
            readerPromise.set_value();
            return ReadTask();  // finish read sequence
        }
        else
        {
            return ReadTask(onReadHeader, sizeof(size_t));
        }
    };

    auto onWriteCounterCallback = [&]()
    {
        if (++sendCounter == toSend.size())
        {
            writerPromise.set_value();
        }
    };

    // trigger reading
    readerSession->scheduleRead(ReadTask(onReadHeader, sizeof(size_t)));

    auto writerThread = std::thread(
        [&]()
        {
            for (size_t i = 0; i < toSend.size(); ++i)
            {
                std::vector<WriteTask> tasks;
                {
                    auto header = std::make_shared<size_t>(toSend[i].size());
                    auto writeHandler = [header]() {};
                    boost::asio::const_buffer buffer(header.get(), sizeof(*header));
                    tasks.push_back(WriteTask(buffer, writeHandler));
                }
                {
                    auto payloadString = std::make_shared<std::string>(toSend[i]);
                    auto writeHandler = [payloadString, onWriteCounterCallback]() { onWriteCounterCallback(); };
                    boost::asio::const_buffer buffer(payloadString->data(), payloadString->size());
                    tasks.push_back(WriteTask(buffer, writeHandler));
                }
                writerSession->scheduleWrite(std::move(tasks));
            }
        });

    EXPECT_EQ(writerFuture.wait_for(timeout), std::future_status::ready);
    EXPECT_EQ(readerFuture.wait_for(timeout), std::future_status::ready);
    EXPECT_EQ(toSend, received);

    if (writerThread.joinable())
    {
        writerThread.join();
    }
}

// Sender sends multiple strings, reader reads multiple strings as messages with header and payload
// data is written from multiple separate threads
TEST_F(MessagesTest, SplitMessagesMultiThread)
{
    auto writerSession = serverSession;
    auto readerSession = clientSession;

    std::promise<void> writerPromise;
    std::future<void> writerFuture = writerPromise.get_future();

    std::promise<void> readerPromise;
    std::future<void> readerFuture = readerPromise.get_future();

    std::vector<std::string> toSend{"odd_one",   "even_two",   "odd_three", "even_four", "odd_five",   "even_six",
                                    "odd_seven", "even_eight", "odd_nine",  "even_ten",  "odd_eleven", "even_twelve",
                                    "odd_one",   "even_two",   "odd_three", "even_four", "odd_five",   "even_six",
                                    "odd_seven", "even_eight", "odd_nine",  "even_ten",  "odd_eleven", "even_twelve"};
    std::vector<std::string> receivedOdd;
    std::vector<std::string> receivedEven;

    size_t sendCounter = 0;
    size_t recvCounter = 0;

    ReadHandler onReadPayload;
    ReadHandler onReadHeader = [&](const void* data, size_t size) -> ReadTask
    {
        size_t nextPayloadSize;
        memcpy(&nextPayloadSize, data, sizeof(nextPayloadSize));
        NS_LOG_T("Read header: size {}, value {}", sizeof(nextPayloadSize), nextPayloadSize);
        return ReadTask(onReadPayload, nextPayloadSize);
    };
    onReadPayload = [&](const void* data, size_t size) -> ReadTask
    {
        auto payload = std::string(reinterpret_cast<const char*>(data), size);
        if (payload.find("odd_") != std::string::npos)
            receivedOdd.push_back(payload);
        else if (payload.find("even_") != std::string::npos)
            receivedEven.push_back(payload);

        NS_LOG_T("Read payload: {}", payload);
        if (++recvCounter == toSend.size())
        {
            readerPromise.set_value();
            return ReadTask();  // finish read sequence
        }
        else
        {
            return ReadTask(onReadHeader, sizeof(size_t));
        }
    };

    auto onWriteCounterCallback = [&]()
    {
        if (++sendCounter == toSend.size())
        {
            writerPromise.set_value();
        }
    };

    // trigger reading
    readerSession->scheduleRead(ReadTask(onReadHeader, sizeof(size_t)));

    auto writeLambda = [&](std::string payload)
    {
        std::vector<WriteTask> tasks;
        {
            auto header = std::make_shared<size_t>(payload.size());
            auto writeHandler = [header]() {};
            boost::asio::const_buffer buffer(header.get(), sizeof(*header));
            tasks.push_back(WriteTask(buffer, writeHandler));
        }
        {
            auto payloadString = std::make_shared<std::string>(payload);
            auto writeHandler = [payloadString, onWriteCounterCallback]() { onWriteCounterCallback(); };
            boost::asio::const_buffer buffer(payloadString->data(), payloadString->size());
            tasks.push_back(WriteTask(buffer, writeHandler));
        }
        writerSession->scheduleWrite(std::move(tasks));
    };

    auto oddWriterLambda = [&]()
    {
        for (size_t i = 0; i < toSend.size(); ++i)
        {
            if (toSend[i].find("odd_") == std::string::npos)
                continue;
            writeLambda(toSend[i]);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };

    auto evenWriterLambda = [&]()
    {
        for (size_t i = 0; i < toSend.size(); ++i)
        {
            if (toSend[i].find("even_") == std::string::npos)
                continue;
            writeLambda(toSend[i]);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };

    auto evenWriterThread = std::thread(evenWriterLambda);
    auto oddWriterThread = std::thread(oddWriterLambda);

    EXPECT_EQ(writerFuture.wait_for(timeout), std::future_status::ready);
    EXPECT_EQ(readerFuture.wait_for(timeout), std::future_status::ready);

    size_t evenIndex = 0;
    size_t oddIndex = 0;
    for (size_t i = 0; i < toSend.size(); ++i)
    {
        if (toSend[i].find("even_") != std::string::npos)
            EXPECT_EQ(toSend[i], receivedEven[evenIndex++]);
        else
            EXPECT_EQ(toSend[i], receivedOdd[oddIndex++]);
    }

    if (oddWriterThread.joinable())
    {
        oddWriterThread.join();
    }

    if (evenWriterThread.joinable())
    {
        evenWriterThread.join();
    }
}

TEST_F(MessagesTest, ConnectionActivityMonitoring)
{
    // much longer than test duration, so only initial pongs will be sent
    const auto heartbeatPeriod = std::chrono::milliseconds(100000);

    size_t clientActivityMarkersCnt = 0;
    auto onClientConnectionAliveCb = [&clientActivityMarkersCnt]() { clientActivityMarkersCnt++; };

    size_t serverActivityMarkersCnt = 0;
    auto onServerConnectionAliveCb = [&serverActivityMarkersCnt]() { serverActivityMarkersCnt++; };

    serverSession->startConnectionActivityMonitoring(onServerConnectionAliveCb, heartbeatPeriod);
    clientSession->startConnectionActivityMonitoring(onClientConnectionAliveCb, heartbeatPeriod);

    auto onServerReadCallback = [&, this](const void*, size_t)
    {
        serverReadPromise.set_value();
        return ReadTask();
    };
    serverSession->scheduleRead(ReadTask(onServerReadCallback, 1));
    auto onClientReadCallback = [&, this](const void*, size_t)
    {
        clientReadPromise.set_value();
        return ReadTask();
    };
    clientSession->scheduleRead(ReadTask(onClientReadCallback, 1));

    std::vector<WriteTask> tasks;
    auto symbol = std::make_shared<char>('a');
    auto writeHandler = [symbol]() {};
    boost::asio::const_buffer buffer(symbol.get(), sizeof(*symbol));
    tasks.push_back(WriteTask(buffer, writeHandler));

    auto serverTasks = tasks;

    serverSession->scheduleWrite(std::move(serverTasks));
    clientSession->scheduleWrite(std::move(tasks));

    EXPECT_EQ(serverReadFuture.wait_for(timeout), std::future_status::ready);
    EXPECT_EQ(clientReadFuture.wait_for(timeout), std::future_status::ready);

    // 1 pong, 1 succedeed read op, write operations ignored
    ASSERT_EQ(clientActivityMarkersCnt, 2u);
    ASSERT_EQ(serverActivityMarkersCnt, 2u);
}

// Test parameters:
// 1st - Number of write batches to schedule
// 2nd - Latency delay in milliseconds introduced after the execution of each task in the batch
// 3rd - Timeout duration in milliseconds for each batched write task
// 4th - How many batched writes should succeed before the timeout occurs due to accumulated delay
class WriteTimeoutTestP : public MessagesTest, public testing::WithParamInterface<std::tuple<size_t, size_t, size_t, size_t>>
{
};

TEST_P(WriteTimeoutTestP, WriteTimeout)
{
    const size_t numBatchedWrites = std::get<0>(GetParam());
    const size_t writeLatencyMs = std::get<1>(GetParam());
    const size_t writeTimeoutMs = std::get<2>(GetParam());
    const size_t expectedCompletedWrites = std::get<3>(GetParam());

    const auto maxTestDuration = std::chrono::milliseconds(numBatchedWrites * writeLatencyMs + 1000);

    using PromisePtr = std::shared_ptr<std::promise<void>>;

    // Promise and future to handle the timeout event
    auto writeTimedOutPromise = std::make_shared<std::promise<void>>();
    std::future<void> writeTimedOutFuture = writeTimedOutPromise->get_future();

    // Set the callback for write timeout error
    auto onWriteTimeoutCallback = [writeTimedOutPromise](const std::string& message, std::shared_ptr<Session>)
    {
        writeTimedOutPromise->set_value();
    };
    serverSession->setWriteTimedOutHandler(onWriteTimeoutCallback);

    size_t actualCompletedWrites = 0;

    // Create the write task with a delay and increment actualCompletedWrites when it finishes
    auto createWriteTask = [&actualCompletedWrites](PromisePtr writePromise, size_t delayMs, char symbol)
    {
        auto data = std::make_shared<char>(symbol);
        auto writeHandler = [data, writePromise, delayMs, &actualCompletedWrites]()
        {
            actualCompletedWrites++;
            writePromise->set_value();
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        };
        boost::asio::const_buffer buffer(data.get(), sizeof(*data));
        return WriteTask(buffer, writeHandler);
    };

    // Store promises and futures for write completion
    std::vector<PromisePtr> writePromises;
    std::vector<std::future<void>> writeFutures;

    for (size_t i = 0; i < numBatchedWrites; ++i)
    {
        auto promise = std::make_shared<std::promise<void>>();
        writePromises.push_back(promise);
        writeFutures.push_back(promise->get_future());

        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(writeTimeoutMs);
        serverSession->scheduleWrite({createWriteTask(promise, writeLatencyMs, 'a')}, deadline);
    }

    // Check that the expected number of write tasks completed successfully
    for (size_t i = 0; i < expectedCompletedWrites; ++i)
    {
        EXPECT_EQ(writeFutures[i].wait_for(maxTestDuration), std::future_status::ready) << "Write task " << i << " did not complete in time";
    }

    // Verify that the actual completed writes match the expected number
    EXPECT_EQ(actualCompletedWrites, expectedCompletedWrites);

    // If fewer writes were completed than scheduled, verify that the timeout occurred
    if (expectedCompletedWrites < numBatchedWrites)
    {
        EXPECT_EQ(writeTimedOutFuture.wait_for(maxTestDuration), std::future_status::ready);
    }
}

INSTANTIATE_TEST_SUITE_P(
    WriteTimeoutTests,
    WriteTimeoutTestP,
    testing::Values(
        std::make_tuple(2, 100, 50, 1),
        std::make_tuple(10, 100, 1100, 10),
        std::make_tuple(10, 100, 900, 9),
        std::make_tuple(10, 100, 899, 8),
        std::make_tuple(10, 100, 699, 6),
        std::make_tuple(10, 100, 199, 1)
    )
);
