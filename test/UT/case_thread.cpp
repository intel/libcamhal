/*
 * Copyright (C) 2018 Intel Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "CASE_THREAD"

#include "case_common.h"
#include "Errors.h"
#include "iutils/Thread.h"
#include "iutils/CameraLog.h"

#include <cstdlib>
#include <queue>
#include <string>

using namespace std;
using namespace icamera;

class SampleThread : public Thread {
public:
    SampleThread() : mSleepTime(0), mLoopTimes(1), mExiting(false) {}
    ~SampleThread() {}

    long mSleepTime;
    int mLoopTimes;
    bool mExiting;

    bool threadLoop() {
        mLoopTimes--;
        if (mSleepTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(mSleepTime));
        }
        return (!mExiting && mLoopTimes > 0);
    }
};

TEST(ThreadTest, basic_create) {
    int ret = OK;

    SampleThread t;
    t.mSleepTime = 0;
    t.mLoopTimes = 1;

    ret =  t.run();
    EXPECT_EQ(OK, ret);

    ret = t.join();
    EXPECT_EQ(OK, ret);

    EXPECT_EQ(0, t.mLoopTimes);
    EXPECT_FALSE(t.isRunning());
}

TEST(ThreadTest, exit_without_waiting) {
    int ret = OK;

    SampleThread t;
    t.mSleepTime = 0;
    t.mLoopTimes = 1;

    ret =  t.run();
    EXPECT_EQ(OK, ret);
    EXPECT_FALSE(t.isExiting());
}

TEST(ThreadTest, start_again_after_exited) {
    int ret = OK;

    SampleThread t;
    t.mSleepTime = 0;
    t.mLoopTimes = 5;

    ret =  t.run();
    EXPECT_EQ(OK, ret);

    ret = t.join();
    EXPECT_EQ(OK, ret);

    EXPECT_EQ(0, t.mLoopTimes);
    EXPECT_FALSE(t.isRunning());

    t.mLoopTimes = 5;
    ret =  t.run();
    EXPECT_EQ(OK, ret);

    ret = t.join();
    EXPECT_EQ(OK, ret);

    EXPECT_EQ(0, t.mLoopTimes);
    EXPECT_FALSE(t.isRunning());
}

TEST(ThreadTest, start_again_during_running) {
    int ret = OK;

    SampleThread t;
    t.mSleepTime = 1;
    t.mLoopTimes = 1000;

    ret =  t.run();
    EXPECT_EQ(OK, ret);

    ret =  t.run();
    EXPECT_NE(OK, ret);

    t.mExiting = true;
    ret = t.join();
    EXPECT_EQ(OK, ret);

    EXPECT_FALSE(t.isRunning());
}

TEST(ThreadTest, exit_by_requestExit) {
    int ret = OK;

    SampleThread t;
    t.mSleepTime = 1;
    t.mLoopTimes = 1000;

    ret =  t.run();
    EXPECT_EQ(OK, ret);

    t.requestExit();
    EXPECT_TRUE(t.isExiting());
    EXPECT_FALSE(t.isExited());
    ret = t.join();
    EXPECT_EQ(OK, ret);

    EXPECT_FALSE(t.isRunning());
    EXPECT_TRUE(t.isExited());
}

TEST(ThreadTest, exit_by_requestExitAndWait) {
    int ret = OK;

    SampleThread t;
    t.mSleepTime = 1;
    t.mLoopTimes = 1000;

    ret =  t.run();
    EXPECT_EQ(OK, ret);

    ret = t.requestExitAndWait();
    EXPECT_EQ(OK, ret);

    EXPECT_FALSE(t.isRunning());
}

TEST(ThreadTest, test_thread_priority) {
    class PriorityTest : public Thread {
    public:
        PriorityTest() : mNumberOfProducts(0) {}
        ~PriorityTest() {}

        long mNumberOfProducts;

        bool threadLoop() {
            mNumberOfProducts++;
            long sum = 0;
            for (long i = 0; i < mNumberOfProducts; i++) {
                sum += i;
            }
            return sum >= 0;
        }
    };

    vector<PriorityTest*> mThreads;
    const int kNumOfThreads = 10;

    PriorityTest* lowPriority = nullptr;
    PriorityTest* highPriority = nullptr;

    for (int i = 0; i < kNumOfThreads; i++) {
        PriorityTest* t = new PriorityTest();
        mThreads.push_back(t);

        // Randomly pick one lowest/highest priority thread
        if (i == 4) {
            lowPriority = t;
            lowPriority->run("LOWEST", PRIORITY_LOWEST);
        } else if (i == 5) {
            highPriority = t;
            highPriority->run("HIGHEST", PRIORITY_HIGHEST);
        } else {
            t->run("DEFAULT", PRIORITY_DEFAULT);
        }
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));

    for (auto& t : mThreads) {
        t->requestExitAndWait();
    }

    EXPECT_TRUE(highPriority->mNumberOfProducts > lowPriority->mNumberOfProducts);
}

TEST(ThreadTest, test_thread_condition_and_mutex) {
    struct ProductData {
        const int kContainerCap = 10;
        queue<int> productList;
        Mutex productLock;
        Condition productProducedSignal;
        Condition productConsumedSignal;
    };

    class Producer : public Thread {
    public:
        string mName;
        int mHasProduced;
        int mNeedProduce;
        long long mTotalPrice;
        ProductData *mProductData;

        Producer(const char* name, ProductData* productData) :
                mName(name), mHasProduced(0), mNeedProduce(1),
                mTotalPrice(0), mProductData(productData) {}
        ~Producer() {}

        bool threadLoop() {
            mHasProduced++;
            if (mHasProduced > mNeedProduce) {
                return false;
            }

            std::this_thread::sleep_for(std::chrono::microseconds(rand() % 10 + 1));

            ConditionLock lock(mProductData->productLock);
            while (mProductData->productList.size() >= mProductData->kContainerCap) {
                mProductData->productConsumedSignal.wait(lock);
            }

            bool needNotify = mProductData->productList.empty();
            // The price of a product is between 1~100.
            int productPrice = rand() % 100 + 1;
            mTotalPrice += productPrice;
            mProductData->productList.push(productPrice);

            if (needNotify) {
                mProductData->productProducedSignal.broadcast();
            }

            return true;
        }
    };

    class Consumer : public Thread {
    public:
        string mName;
        bool mExiting;
        long long mTotalCost;
        ProductData* mProductData;

        Consumer (const char* name, ProductData* productData) :
                mName(name), mExiting(false), mTotalCost(0), mProductData(productData) {}
        ~Consumer() {}

        void exit() {
            AutoMutex lock(mProductData->productLock);
            mExiting = true;
            mProductData->productProducedSignal.broadcast();
        }

        bool threadLoop() {
            std::this_thread::sleep_for(std::chrono::microseconds(rand() % 10 + 1));

            ConditionLock lock(mProductData->productLock);
            while (mProductData->productList.empty()) {
                if (mExiting) return false;

                mProductData->productProducedSignal.wait(lock);
            }

            bool needNotify = mProductData->productList.size() >= mProductData->kContainerCap;
            mTotalCost += mProductData->productList.front();
            mProductData->productList.pop();
            if (needNotify) {
                mProductData->productConsumedSignal.broadcast();
            }

            return true;
        }
    };

    ProductData* productData = new ProductData();

    // Choose random number of the producers and consumers.
    const int kNumOfProducers = rand() % 10 + 1;
    const int kNumOfConsumers = rand() % 10 + 1;
    vector<Producer*> producers;
    vector<Consumer*> consumers;

    for (int i = 0; i < kNumOfProducers; i++) {
        producers.push_back(new Producer("Producer", productData));
    }

    for (int i = 0; i < kNumOfConsumers; i++) {
        consumers.push_back(new Consumer("Consumer", productData));
    }

    for (auto& p : producers) {
        // Each producer randomly produces 5000~10000 products.
        p->mNeedProduce = rand() % 5000 + 5000;
        p->run();
    }

    for (auto& c : consumers) c->run();

    for (auto& p : producers) p->join();

    for (auto& c : consumers) c->exit();
    for (auto& c : consumers) c->join();

    long long totalPrice = 0, totalCost = 0;
    for (auto& p : producers) totalPrice += p->mTotalPrice;
    for (auto& c : consumers) totalCost += c->mTotalCost;

    EXPECT_TRUE(productData->productList.empty());
    EXPECT_EQ(totalPrice, totalCost);

    for (auto& p : producers) delete p;
    for (auto& c : consumers) delete c;
    delete productData;
}

