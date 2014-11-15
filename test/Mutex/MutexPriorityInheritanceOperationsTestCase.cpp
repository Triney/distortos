/**
 * \file
 * \brief MutexPriorityInheritanceOperationsTestCase class implementation
 *
 * \author Copyright (C) 2014 Kamil Szczygiel http://www.distortec.com http://www.freddiechopin.info
 *
 * \par License
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * \date 2014-11-15
 */

#include "MutexPriorityInheritanceOperationsTestCase.hpp"

#include "distortos/Mutex.hpp"
#include "distortos/ThisThread.hpp"
#include "distortos/StaticThread.hpp"

#include "distortos/estd/ReferenceHolder.hpp"

namespace distortos
{

namespace test
{

namespace
{

/*---------------------------------------------------------------------------------------------------------------------+
| local types
+---------------------------------------------------------------------------------------------------------------------*/

/// functor class used in testBasicPriorityInheritance() - it locks 0-3 mutexes and unlocks them afterwards
class BasicPriorityInheritanceThread
{
public:

	/**
	 * \brief BasicPriorityInheritanceThread's constructor
	 *
	 * \param [in] mutex1 is a pointer to first mutex
	 * \param [in] mutex2 is a pointer to second mutex
	 * \param [in] mutex3 is a pointer to third mutex
	 */

	constexpr BasicPriorityInheritanceThread(Mutex* const mutex1, Mutex* const mutex2, Mutex* const mutex3) :
			mutexes_{mutex1, mutex2, mutex3},
			ret_{}
	{

	}

	/**
	 * \return combined return value of Mutex::lock() / Mutex::unlock()
	 */

	int getRet() const
	{
		return ret_;
	}

	/**
	 * \brief Main function of the thread.
	 *
	 * Locks all provided mutexes and then unlocks them in the same order.
	 */

	void operator()()
	{
		for (const auto mutex : mutexes_)
			if (mutex != nullptr)
			{
				const auto ret = mutex->lock();
				if (ret != 0)
					ret_ = ret;
			}

		for (const auto mutex : mutexes_)
			if (mutex != nullptr)
			{
				const auto ret = mutex->unlock();
				if (ret != 0)
					ret_ = ret;
			}
	}

private:

	/// array with pointers to mutexes
	std::array<Mutex*, 3> mutexes_;

	/// combined return value of Mutex::lock() / Mutex::unlock()
	int ret_;
};

/*---------------------------------------------------------------------------------------------------------------------+
| local constants
+---------------------------------------------------------------------------------------------------------------------*/

/// priority of current test thread
constexpr uint8_t testThreadPriority {1};

/*---------------------------------------------------------------------------------------------------------------------+
| local functions
+---------------------------------------------------------------------------------------------------------------------*/

/**
 * \brief Tests basic priority inheritance mechanism of mutexes with PriorityInheritance protocol.
 *
 * 10 threads are created and "connected" into a tree-like hierarchy using mutexes. This structure is presented on the
 * diagram below. Mutexes are marked with "Mx" boxes and threads with "Tx" ellipses. The higher the thread is drawn, the
 * higher its priority is (drawn on the side).
 *
 * \dot
 * digraph G
 * {
 * 		{
 * 			node [shape=plaintext];
 * 			"+10" -> "+9" -> "+8" -> "+7" -> "+6" -> "+5" -> "+4" -> "+3" -> "+2" -> "+1" -> "+0";
 * 		}
 *
 * 		{rank = same; "+10"; "T111";}
 * 		{rank = same; "+9"; "T110";}
 * 		{rank = same; "+8"; "T101";}
 * 		{rank = same; "+7"; "T100";}
 * 		{rank = same; "+6"; "T11";}
 * 		{rank = same; "+5"; "T10";}
 * 		{rank = same; "+4"; "T01";}
 * 		{rank = same; "+3"; "T00";}
 * 		{rank = same; "+2"; "T1";}
 * 		{rank = same; "+1"; "T0";}
 * 		{rank = same; "+0"; "main";}
 *
 * 		{
 * 			node [shape=box];
 * 			"M0"; "M1"; "M00"; "M01"; "M10"; "M11"; "M100"; "M101"; "M110"; "M111";
 * 		}
 *
 * 		"T111" -> "M111" -> "T11" -> "M11" -> "T1" -> "M1" -> "main";
 * 		"T110" -> "M110" -> "T11";
 * 		"T101" -> "M101" -> "T10" -> "M10" -> "T1";
 * 		"T100" -> "M100" -> "T10";
 * 		"T01" -> "M01" -> "T0" -> "M0" -> "main";
 * 		"T00" -> "M00" -> "T0";
 * }
 * \enddot
 *
 * Main thread is expected to inherit priority of each started test thread when this thread blocks on the mutex. After
 * the last step main thread will inherit priority of thread T111 through a chain of 3 mutexes blocking 3 threads. After
 * the test (when all links are broken) all priorities are expected to return to their previous values.
 *
 * \param [in] type is the Mutex::Type that will be tested
 *
 * \return true if the test case succeeded, false otherwise
 */

bool testBasicPriorityInheritance(const Mutex::Type type)
{
	constexpr size_t testThreadStackSize {256};
	constexpr size_t totalThreads {10};

	// effective priority (relative to testThreadPriority) for each test thread in each test step
	static const std::array<std::array<uint8_t, totalThreads>, totalThreads> priorityBoosts
	{{
			{1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
			{1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
			{3, 2, 3, 4, 5, 6, 7, 8, 9, 10},
			{4, 2, 3, 4, 5, 6, 7, 8, 9, 10},
			{4, 5, 3, 4, 5, 6, 7, 8, 9, 10},
			{4, 6, 3, 4, 5, 6, 7, 8, 9, 10},
			{4, 7, 3, 4, 7, 6, 7, 8, 9, 10},
			{4, 8, 3, 4, 8, 6, 7, 8, 9, 10},
			{4, 9, 3, 4, 8, 9, 7, 8, 9, 10},
			{4, 10, 3, 4, 8, 10, 7, 8, 9, 10},
	}};

	Mutex mutex0 {type, Mutex::Protocol::PriorityInheritance};
	Mutex mutex1 {type, Mutex::Protocol::PriorityInheritance};
	Mutex mutex00 {type, Mutex::Protocol::PriorityInheritance};
	Mutex mutex01 {type, Mutex::Protocol::PriorityInheritance};
	Mutex mutex10 {type, Mutex::Protocol::PriorityInheritance};
	Mutex mutex11 {type, Mutex::Protocol::PriorityInheritance};
	Mutex mutex100 {type, Mutex::Protocol::PriorityInheritance};
	Mutex mutex101 {type, Mutex::Protocol::PriorityInheritance};
	Mutex mutex110 {type, Mutex::Protocol::PriorityInheritance};
	Mutex mutex111 {type, Mutex::Protocol::PriorityInheritance};

	BasicPriorityInheritanceThread threadObject0 {&mutex00, &mutex01, &mutex0};
	BasicPriorityInheritanceThread threadObject1 {&mutex10, &mutex11, &mutex1};
	BasicPriorityInheritanceThread threadObject00 {&mutex00, nullptr, nullptr};
	BasicPriorityInheritanceThread threadObject01 {&mutex01, nullptr, nullptr};
	BasicPriorityInheritanceThread threadObject10 {&mutex100, &mutex101, &mutex10};
	BasicPriorityInheritanceThread threadObject11 {&mutex110, &mutex111, &mutex11};
	BasicPriorityInheritanceThread threadObject100 {&mutex100, nullptr, nullptr};
	BasicPriorityInheritanceThread threadObject101 {&mutex101, nullptr, nullptr};
	BasicPriorityInheritanceThread threadObject110 {&mutex110, nullptr, nullptr};
	BasicPriorityInheritanceThread threadObject111 {&mutex111, nullptr, nullptr};

	auto thread0 =
			makeStaticThread<testThreadStackSize>(testThreadPriority + priorityBoosts[0][0], std::ref(threadObject0));
	auto thread1 =
			makeStaticThread<testThreadStackSize>(testThreadPriority + priorityBoosts[0][1], std::ref(threadObject1));
	auto thread00 =
			makeStaticThread<testThreadStackSize>(testThreadPriority + priorityBoosts[0][2], std::ref(threadObject00));
	auto thread01 =
			makeStaticThread<testThreadStackSize>(testThreadPriority + priorityBoosts[0][3], std::ref(threadObject01));
	auto thread10 =
			makeStaticThread<testThreadStackSize>(testThreadPriority + priorityBoosts[0][4], std::ref(threadObject10));
	auto thread11 =
			makeStaticThread<testThreadStackSize>(testThreadPriority + priorityBoosts[0][5], std::ref(threadObject11));
	auto thread100 =
			makeStaticThread<testThreadStackSize>(testThreadPriority + priorityBoosts[0][6], std::ref(threadObject100));
	auto thread101 =
			makeStaticThread<testThreadStackSize>(testThreadPriority + priorityBoosts[0][7], std::ref(threadObject101));
	auto thread110 =
			makeStaticThread<testThreadStackSize>(testThreadPriority + priorityBoosts[0][8], std::ref(threadObject110));
	auto thread111 =
			makeStaticThread<testThreadStackSize>(testThreadPriority + priorityBoosts[0][9], std::ref(threadObject111));

	std::array<estd::ReferenceHolder<decltype(thread0)>, totalThreads> threads
	{{
			thread0, thread1, thread00, thread01, thread10, thread11, thread100, thread101, thread110, thread111,
	}};

	bool result {true};

	{
		const auto ret = mutex0.lock();
		if (ret != 0)
			result = false;
	}
	{
		const auto ret = mutex1.lock();
		if (ret != 0)
			result = false;
	}

	for (size_t i = 0; i < threads.size(); ++i)
	{
		auto& thread = threads[i].get();
		thread.start();
		if (ThisThread::getEffectivePriority() != thread.getEffectivePriority())
			result = false;

		for (size_t j = 0; j < threads.size(); ++j)
			if (threads[j].get().getEffectivePriority() != testThreadPriority + priorityBoosts[i][j])
				result = false;
	}

	{
		const auto ret = mutex1.unlock();
		if (ret != 0)
			result = false;
	}

	if (ThisThread::getEffectivePriority() != thread0.getEffectivePriority())
		result = false;

	{
		const auto ret = mutex0.unlock();
		if (ret != 0)
			result = false;
	}

	for (const auto& thread : threads)
		thread.get().join();

	if (ThisThread::getEffectivePriority() != testThreadPriority)
		result = false;

	for (size_t i = 0; i < threads.size(); ++i)
		if (threads[i].get().getEffectivePriority() != testThreadPriority + priorityBoosts[0][i])
			result = false;

	for (const auto& threadObject : {threadObject0, threadObject1, threadObject00, threadObject01, threadObject10,
			threadObject11, threadObject100, threadObject101, threadObject110, threadObject111})
		if (threadObject.getRet() != 0)
			result = false;

	return result;
}

/**
 * \brief Runs the test case.
 *
 * \attention this function expects the priority of test thread to be testThreadPriority
 *
 * \return true if the test case succeeded, false otherwise
 */

bool testRunner()
{
	static const Mutex::Type types[]
	{
			Mutex::Type::Normal,
			Mutex::Type::ErrorChecking,
			Mutex::Type::Recursive,
	};

	for (const auto type : types)
	{
		{
			const auto result = testBasicPriorityInheritance(type);
			if (result != true)
				return result;
		}
	}

	return true;
}

}	// namespace

/*---------------------------------------------------------------------------------------------------------------------+
| private functions
+---------------------------------------------------------------------------------------------------------------------*/

bool MutexPriorityInheritanceOperationsTestCase::run_() const
{
	const auto thisThreadPriority = ThisThread::getPriority();
	ThisThread::setPriority(testThreadPriority);
	const auto ret = testRunner();
	ThisThread::setPriority(thisThreadPriority);
	return ret;
}

}	// namespace test

}	// namespace distortos
