/*
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 * 
 *  http://aws.amazon.com/apache2.0
 * 
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <aws/external/gtest.h>
#include <aws/testing/ProxyConfig.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/client/CoreErrors.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/core/utils/xml/XmlSerializer.h>
#include <aws/sqs/SQSClient.h>
#include <aws/sqs/model/CreateQueueRequest.h>
#include <aws/sqs/model/ListQueuesRequest.h>
#include <aws/sqs/model/DeleteQueueRequest.h>
#include <aws/sqs/model/SendMessageRequest.h>
#include <aws/sqs/model/ReceiveMessageRequest.h>
#include <aws/sqs/model/DeleteMessageRequest.h>
#include <aws/sqs/model/GetQueueAttributesRequest.h>
#include <aws/sqs/model/SetQueueAttributesRequest.h>
#include <aws/sqs/model/AddPermissionRequest.h>
#include <aws/sqs/model/RemovePermissionRequest.h>
#include <aws/sqs/model/ListDeadLetterSourceQueuesRequest.h>
#include <aws/core/utils/Outcome.h>
#include <aws/testing/ProxyConfig.h>

using namespace Aws::Http;
using namespace Aws;
using namespace Aws::Auth;
using namespace Aws::Client;
using namespace Aws::SQS;
using namespace Aws::SQS::Model;
using namespace Aws::Utils::Json;

#define TEST_QUEUE_PREFIX "IntegrationTest_"

//we need a way to pull this at runtime.
static const char* ACCOUNT_ID = "554229317296";
static const char* SIMPLE_QUEUE_NAME = TEST_QUEUE_PREFIX "Simple";
static const char* SEND_RECEIVE_QUEUE_NAME = TEST_QUEUE_PREFIX "SendReceive";
static const char* ATTRIBUTES_QUEUE_NAME = TEST_QUEUE_PREFIX "Attributes";
static const char* PERMISSIONS_QUEUE_NAME = TEST_QUEUE_PREFIX "Permissions";
static const char* DEAD_LETTER_QUEUE_NAME = TEST_QUEUE_PREFIX "DeadLetter";
static const char* DEAD_LETTER_SOURCE_QUEUE_NAME = TEST_QUEUE_PREFIX "DeadLetterSource";
static const char* ALLOCATION_TAG = "QueueOperationTest";

namespace
{

class QueueOperationTest : public ::testing::Test
{

public:
    std::shared_ptr<SQSClient> sqsClient;
    Aws::String m_accountId;

protected:
    virtual void SetUp()
    {
        m_accountId = ProfileConfigFileAWSCredentialsProvider::GetAccountIdForProfile("default");
        if (m_accountId.size() == 0)
        {
            m_accountId = ACCOUNT_ID;
        }

        ClientConfiguration config;
        config.scheme = Scheme::HTTPS;
        config.region = Region::US_EAST_1;

#if USE_PROXY_FOR_TESTS
        config.scheme = Scheme::HTTP;
        config.proxyHost = PROXY_HOST;
        config.proxyPort = PROXY_PORT;
#endif
        sqsClient = Aws::MakeShared<SQSClient>(ALLOCATION_TAG, Aws::MakeShared<DefaultAWSCredentialsProviderChain>(ALLOCATION_TAG), config);

        // delete queues, just in case
        DeleteAllTestQueues();
    }

    virtual void TearDown()
    {
        // delete queues, just in case
        DeleteAllTestQueues();
        sqsClient = nullptr;
    }

    Aws::String CreateDefaultQueue(Aws::String name)
    {
        CreateQueueRequest request;
        request.SetQueueName(name);

        bool shouldContinue = true;
        while (shouldContinue)
        {
            CreateQueueOutcome outcome = sqsClient->CreateQueue(request);
            if (outcome.IsSuccess())
            {
                return outcome.GetResult().GetQueueUrl();
            }
            if (outcome.GetError().GetErrorType() != SQSErrors::QUEUE_DELETED_RECENTLY)
            {
                return "";
            }
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }

        return "";
    }

    void DeleteAllTestQueues()
    {
        ListQueuesRequest listQueueRequest;
        listQueueRequest.WithQueueNamePrefix(TEST_QUEUE_PREFIX);

        ListQueuesOutcome listQueuesOutcome = sqsClient->ListQueues(listQueueRequest);
        ListQueuesResult listQueuesResult = listQueuesOutcome.GetResult();
        Aws::Vector<Aws::String> urls = listQueuesResult.GetQueueUrls();
        for (auto& url : listQueuesResult.GetQueueUrls())
        {
            DeleteQueueRequest deleteQueueRequest;
            deleteQueueRequest.WithQueueUrl(url);
            DeleteQueueOutcome deleteQueueOutcome = sqsClient->DeleteQueue(deleteQueueRequest);
        }

        bool done = false;
        while(!done)
        {
            listQueuesOutcome = sqsClient->ListQueues(listQueueRequest);
            listQueuesResult = listQueuesOutcome.GetResult();
            if(listQueuesResult.GetQueueUrls().size() == 0)
            {
                break;
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    void CreateQueue(const Aws::String name)
    {
        CreateQueueRequest createQueueRequest;
        createQueueRequest.SetQueueName(name);

    }
};
} // anonymous namespace

TEST_F(QueueOperationTest, TestCreateAndDeleteQueue)
{
    CreateQueueRequest createQueueRequest;
    createQueueRequest.SetQueueName(SIMPLE_QUEUE_NAME);

    CreateQueueOutcome createQueueOutcome;
    bool shouldContinue = true;
    while (shouldContinue)
    {
        createQueueOutcome = sqsClient->CreateQueue(createQueueRequest);
        if (createQueueOutcome.IsSuccess()) break;
        if (createQueueOutcome.GetError().GetErrorType() == SQSErrors::QUEUE_DELETED_RECENTLY)
        {
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
        else
        {
            FAIL() << "Unexpected error response: " << createQueueOutcome.GetError().GetMessage();
        }
    }

    Aws::String queueUrl = createQueueOutcome.GetResult().GetQueueUrl();

    ASSERT_TRUE(queueUrl.find(createQueueRequest.GetQueueName()) != Aws::String::npos);

    createQueueRequest.AddAttributes(QueueAttributeName::VisibilityTimeout, "50");

    createQueueOutcome = sqsClient->CreateQueue(createQueueRequest);
    ASSERT_FALSE(createQueueOutcome.IsSuccess());
    SQSErrors error = createQueueOutcome.GetError().GetErrorType();
    EXPECT_TRUE(SQSErrors::QUEUE_NAME_EXISTS == error || SQSErrors::QUEUE_DELETED_RECENTLY == error);


    // This call in eventually consistent (sometimes over 1 min), so try it a few times
    for (int attempt = 0; ; attempt++)
    {
        ListQueuesRequest listQueueRequest;
        listQueueRequest.WithQueueNamePrefix(TEST_QUEUE_PREFIX);

        ListQueuesOutcome listQueuesOutcome = sqsClient->ListQueues(listQueueRequest);
        if (listQueuesOutcome.IsSuccess())
        {
            ListQueuesResult listQueuesResult = listQueuesOutcome.GetResult();
            if (listQueuesResult.GetQueueUrls().size() == 1)
            {
                EXPECT_EQ(queueUrl, listQueuesResult.GetQueueUrls()[0]);
                EXPECT_TRUE(listQueuesResult.GetResponseMetadata().GetRequestId().length() > 0);
                break; // success!
            }
        }
        if (attempt >= 10) FAIL();
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    DeleteQueueRequest deleteQueueRequest;
    deleteQueueRequest.WithQueueUrl(queueUrl);

    DeleteQueueOutcome deleteQueueOutcome = sqsClient->DeleteQueue(deleteQueueRequest);
    ASSERT_TRUE(deleteQueueOutcome.IsSuccess());
}

TEST_F(QueueOperationTest, TestSendReceiveDelete)
{
    Aws::String queueUrl = CreateDefaultQueue(SEND_RECEIVE_QUEUE_NAME);
    ASSERT_TRUE(queueUrl.find(SEND_RECEIVE_QUEUE_NAME) != Aws::String::npos);

    SendMessageRequest sendMessageRequest;
    sendMessageRequest.SetMessageBody("TestMessageBody");
    MessageAttributeValue stringAttributeValue;
    stringAttributeValue.SetStringValue("TestString");
    stringAttributeValue.SetDataType("String");
    sendMessageRequest.AddMessageAttributes("TestStringAttribute", stringAttributeValue);

    MessageAttributeValue binaryAttributeValue;
    Aws::Utils::ByteBuffer byteBuffer(10);
    for(unsigned i = 0; i < 10; ++i)
    {
        byteBuffer[i] = (unsigned char)i;
    }

    binaryAttributeValue.SetBinaryValue(byteBuffer);
    binaryAttributeValue.SetDataType("Binary");
    sendMessageRequest.AddMessageAttributes("TestBinaryAttribute", binaryAttributeValue);

    sendMessageRequest.SetQueueUrl(queueUrl);

    SendMessageOutcome sendMessageOutcome = sqsClient->SendMessage(sendMessageRequest);
    ASSERT_TRUE(sendMessageOutcome.IsSuccess());
    EXPECT_TRUE(sendMessageOutcome.GetResult().GetMessageId().length() > 0);

    ReceiveMessageRequest receiveMessageRequest;
    receiveMessageRequest.SetMaxNumberOfMessages(1);
    receiveMessageRequest.SetQueueUrl(queueUrl);
    receiveMessageRequest.AddMessageAttributeNames("All");

    ReceiveMessageOutcome receiveMessageOutcome = sqsClient->ReceiveMessage(receiveMessageRequest);
    ASSERT_TRUE(receiveMessageOutcome.IsSuccess());
    ReceiveMessageResult receiveMessageResult = receiveMessageOutcome.GetResult();
    ASSERT_EQ(1uL, receiveMessageResult.GetMessages().size());
    EXPECT_EQ("TestMessageBody", receiveMessageResult.GetMessages()[0].GetBody());
    Aws::Map<Aws::String, MessageAttributeValue> messageAttributes = receiveMessageResult.GetMessages()[0].GetMessageAttributes();
    ASSERT_TRUE(messageAttributes.find("TestStringAttribute") != messageAttributes.end());
    EXPECT_EQ(stringAttributeValue.GetStringValue(), messageAttributes["TestStringAttribute"].GetStringValue());
    ASSERT_TRUE(messageAttributes.find("TestBinaryAttribute") != messageAttributes.end());
    EXPECT_EQ(byteBuffer, messageAttributes["TestBinaryAttribute"].GetBinaryValue());

    DeleteMessageRequest deleteMessageRequest;
    deleteMessageRequest.SetQueueUrl(queueUrl);
    deleteMessageRequest.SetReceiptHandle(receiveMessageResult.GetMessages()[0].GetReceiptHandle());

    DeleteMessageOutcome deleteMessageOutcome = sqsClient->DeleteMessage(deleteMessageRequest);
    ASSERT_TRUE(deleteMessageOutcome.IsSuccess());

    receiveMessageOutcome = sqsClient->ReceiveMessage(receiveMessageRequest);
    EXPECT_EQ(0uL, receiveMessageOutcome.GetResult().GetMessages().size());

    DeleteQueueRequest deleteQueueRequest;
    deleteQueueRequest.WithQueueUrl(queueUrl);

    DeleteQueueOutcome deleteQueueOutcome = sqsClient->DeleteQueue(deleteQueueRequest);
    ASSERT_TRUE(deleteQueueOutcome.IsSuccess());
}


TEST_F(QueueOperationTest, TestQueueAttributes)
{
    CreateQueueRequest createQueueRequest;
    createQueueRequest.SetQueueName(ATTRIBUTES_QUEUE_NAME);
    createQueueRequest.AddAttributes(QueueAttributeName::DelaySeconds, "45");

    CreateQueueOutcome createQueueOutcome = sqsClient->CreateQueue(createQueueRequest);
    ASSERT_TRUE(createQueueOutcome.IsSuccess());
    Aws::String queueUrl = createQueueOutcome.GetResult().GetQueueUrl();
    ASSERT_TRUE(queueUrl.find(createQueueRequest.GetQueueName()) != Aws::String::npos);

    GetQueueAttributesRequest queueAttributesRequest;
    queueAttributesRequest.AddAttributeNames(QueueAttributeName::DelaySeconds).WithQueueUrl(queueUrl);
    GetQueueAttributesOutcome queueAttributesOutcome = sqsClient->GetQueueAttributes(queueAttributesRequest);
    ASSERT_TRUE(queueAttributesOutcome.IsSuccess());
    EXPECT_EQ("45", queueAttributesOutcome.GetResult().GetAttributes().find(QueueAttributeName::DelaySeconds)->second);

    SetQueueAttributesRequest setQueueAttributesRequest;
    setQueueAttributesRequest.AddAttributes(QueueAttributeName::VisibilityTimeout, "42").WithQueueUrl(queueUrl);
    SetQueueAttributesOutcome setQueueAttributesOutcome = sqsClient->SetQueueAttributes(setQueueAttributesRequest);
    ASSERT_TRUE(setQueueAttributesOutcome.IsSuccess());

    queueAttributesRequest.AddAttributeNames(QueueAttributeName::VisibilityTimeout).WithQueueUrl(queueUrl);
    queueAttributesOutcome = sqsClient->GetQueueAttributes(queueAttributesRequest);
    ASSERT_TRUE(queueAttributesOutcome.IsSuccess());
    EXPECT_EQ("45", queueAttributesOutcome.GetResult().GetAttributes().find(QueueAttributeName::DelaySeconds)->second);
    EXPECT_EQ("42", queueAttributesOutcome.GetResult().GetAttributes().find(QueueAttributeName::VisibilityTimeout)->second);

    DeleteQueueRequest deleteQueueRequest;
    deleteQueueRequest.WithQueueUrl(queueUrl);

    DeleteQueueOutcome deleteQueueOutcome = sqsClient->DeleteQueue(deleteQueueRequest);
    ASSERT_TRUE(deleteQueueOutcome.IsSuccess());
}

TEST_F(QueueOperationTest, TestPermissions)
{
    Aws::String queueUrl = CreateDefaultQueue(PERMISSIONS_QUEUE_NAME);
    ASSERT_TRUE(queueUrl.find(PERMISSIONS_QUEUE_NAME) != Aws::String::npos);

    AddPermissionRequest addPermissionRequest;
    addPermissionRequest.AddAWSAccountIds(m_accountId).AddActions("ReceiveMessage").WithLabel("Test").WithQueueUrl(
            queueUrl);
    AddPermissionOutcome permissionOutcome = sqsClient->AddPermission(addPermissionRequest);
    ASSERT_TRUE(permissionOutcome.IsSuccess());

    GetQueueAttributesRequest queueAttributesRequest;
    queueAttributesRequest.AddAttributeNames(QueueAttributeName::Policy).WithQueueUrl(queueUrl);
    GetQueueAttributesOutcome queueAttributesOutcome = sqsClient->GetQueueAttributes(queueAttributesRequest);
    ASSERT_TRUE(queueAttributesOutcome.IsSuccess());

    Aws::String policyString = queueAttributesOutcome.GetResult().GetAttributes().find(QueueAttributeName::Policy)->second;
    EXPECT_TRUE(policyString.length() > 0);
    JsonValue policy(policyString);
    EXPECT_EQ(addPermissionRequest.GetLabel(), policy.GetArray("Statement")[0].GetString("Sid"));
    EXPECT_EQ(m_accountId, policy.GetArray("Statement")[0].GetObject("Principal").GetString("AWS"));
    EXPECT_EQ("SQS:ReceiveMessage", policy.GetArray("Statement")[0].GetString("Action"));

    RemovePermissionRequest removePermissionRequest;
    removePermissionRequest.WithLabel("Test").WithQueueUrl(queueUrl);
    RemovePermissionOutcome removePermissionOutcome = sqsClient->RemovePermission(removePermissionRequest);
    ASSERT_TRUE(removePermissionOutcome.IsSuccess());

    queueAttributesOutcome = sqsClient->GetQueueAttributes(queueAttributesRequest);
    ASSERT_TRUE(queueAttributesOutcome.IsSuccess());
    policyString = queueAttributesOutcome.GetResult().GetAttributes().find(QueueAttributeName::Policy)->second;
    EXPECT_TRUE(policyString.length() > 0);
    JsonValue emptyPolicy(policyString);
    EXPECT_EQ(0uL, emptyPolicy.GetArray("Statement").GetLength());

    DeleteQueueRequest deleteQueueRequest;
    deleteQueueRequest.WithQueueUrl(queueUrl);

    DeleteQueueOutcome deleteQueueOutcome = sqsClient->DeleteQueue(deleteQueueRequest);
    ASSERT_TRUE(deleteQueueOutcome.IsSuccess());
}

TEST_F(QueueOperationTest, TestListDeadLetterSourceQueues)
{
    CreateQueueRequest createQueueRequest;
    createQueueRequest.SetQueueName(DEAD_LETTER_SOURCE_QUEUE_NAME);

    CreateQueueOutcome createQueueOutcome = sqsClient->CreateQueue(createQueueRequest);
    ASSERT_TRUE(createQueueOutcome.IsSuccess());
    Aws::String queueUrl = createQueueOutcome.GetResult().GetQueueUrl();

    createQueueRequest.SetQueueName(DEAD_LETTER_QUEUE_NAME);
    createQueueOutcome = sqsClient->CreateQueue(createQueueRequest);
    ASSERT_TRUE(createQueueOutcome.IsSuccess());
    Aws::String deadLetterQueueUrl = createQueueOutcome.GetResult().GetQueueUrl();

    GetQueueAttributesRequest queueAttributesRequest;
    queueAttributesRequest.AddAttributeNames(QueueAttributeName::QueueArn).WithQueueUrl(deadLetterQueueUrl);
    GetQueueAttributesOutcome queueAttributesOutcome = sqsClient->GetQueueAttributes(queueAttributesRequest);
    ASSERT_TRUE(queueAttributesOutcome.IsSuccess());
    Aws::String redrivePolicy = "{\"maxReceiveCount\":\"5\", \"deadLetterTargetArn\":\""
            + queueAttributesOutcome.GetResult().GetAttributes().find(QueueAttributeName::QueueArn)->second + "\"}";

    SetQueueAttributesRequest setQueueAttributesRequest;
    setQueueAttributesRequest.AddAttributes(QueueAttributeName::RedrivePolicy, redrivePolicy).WithQueueUrl(queueUrl);
    SetQueueAttributesOutcome setQueueAttributesOutcome = sqsClient->SetQueueAttributes(setQueueAttributesRequest);
    ASSERT_TRUE(setQueueAttributesOutcome.IsSuccess());

    ListDeadLetterSourceQueuesRequest listDeadLetterQueuesRequest;
    listDeadLetterQueuesRequest.WithQueueUrl(deadLetterQueueUrl);
    ListDeadLetterSourceQueuesOutcome listDeadLetterQueuesOutcome = sqsClient->ListDeadLetterSourceQueues(listDeadLetterQueuesRequest);
    ASSERT_TRUE(listDeadLetterQueuesOutcome.IsSuccess());
    ASSERT_EQ(1uL, listDeadLetterQueuesOutcome.GetResult().GetQueueUrls().size());
    EXPECT_EQ(queueUrl, listDeadLetterQueuesOutcome.GetResult().GetQueueUrls()[0]);

    DeleteQueueRequest deleteQueueRequest;
    deleteQueueRequest.WithQueueUrl(queueUrl);

    DeleteQueueOutcome deleteQueueOutcome = sqsClient->DeleteQueue(deleteQueueRequest);
    ASSERT_TRUE(deleteQueueOutcome.IsSuccess());

    deleteQueueRequest.WithQueueUrl(deadLetterQueueUrl);
    deleteQueueOutcome = sqsClient->DeleteQueue(deleteQueueRequest);
    ASSERT_TRUE(deleteQueueOutcome.IsSuccess());
}