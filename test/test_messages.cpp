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
        onRWErrorCallback = [&](const boost::system::error_code& ec)
        {
            EXPECT_NE(ec, boost::system::error_code()) << "Success shouldn't be reported via error callback";
            ADD_FAILURE() << "R/W operation failed: " << ec.what();
        };
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

        server = std::make_shared<Server>(onNewServerSessionCallback, ioContextPtrServer, logCallback);
        server->start(CONNECTION_PORT);
        client = std::make_shared<Client>(CONNECTION_HOST,
                                          std::to_string(CONNECTION_PORT),
                                          CONNECTION_PATH,
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

    OnCompleteCallback onRWErrorCallback;
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
    serverSession->scheduleWrite(tasks);
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
    clientSession->scheduleWrite(tasks);
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
        serverSession->scheduleWrite(tasks);
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
        clientSession->scheduleWrite(tasks);
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
                writerSession->scheduleWrite(tasks);
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
        writerSession->scheduleWrite(tasks);
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
