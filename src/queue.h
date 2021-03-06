#pragma once
// *******************************************************************************************
// This file is a part of VCFShark software distributed under GNU GPL 3 licence.
// The homepage of the VCFShark project is https://github.com/refresh-bio/VCFShark
//
// Author : Sebastian Deorowicz and Agnieszka Danek
// Version: 1.0
// Date   : 2020-12-18
// *******************************************************************************************

// Generic multithreading queues

#include <queue>
#include <list>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <map>

using namespace std;

// *****************************************************************************************
//
class CSemaphore {
protected:
	int counter;
	int generation;
	std::mutex mutex;
	std::condition_variable cv;

public:
	// *****************************************************************************************
	//
	CSemaphore(int _counter = 0) : counter(_counter), generation(0) {}

	// *****************************************************************************************
	//
	void Inc(int new_generation = 0) {
		std::unique_lock<std::mutex> lk(mutex);
		if(generation == new_generation)
			++counter;
		else
		{
			generation = new_generation;
			counter = 1;
		}
	}

	// *****************************************************************************************
	//
	void IncNum(int num, int new_generation = 0) {
		std::unique_lock<std::mutex> lk(mutex);
		if(generation == new_generation)
			counter += num;
		else
		{
			generation = new_generation;
			counter = num;
		}
	}

	// *****************************************************************************************
	//
	void Dec(int dec_generation = 0) {
		std::unique_lock<std::mutex> lk(mutex);
		
		if(generation == dec_generation)
			--counter;

		if (counter == 0)
			cv.notify_one();
	}

	// *****************************************************************************************
	//
	void Dec_notify_all(int dec_generation = 0) {
		std::unique_lock<std::mutex> lk(mutex);
		if(generation == dec_generation)
			--counter;

		if(counter == 0)
			cv.notify_all();
	}

	// *****************************************************************************************
	//
	void WaitForZero(int wait_generation = 0) {
		std::unique_lock<std::mutex> lk(mutex);
		cv.wait(lk, [this, &wait_generation] {
			return (counter == 0) && (generation == wait_generation);});
	}
};

// ************************************************************************************
// Multithreading queue with registering mechanism:
//   * The queue can report whether it is in wainitng for new data state or there will be no new data
template<typename T> class CRegisteringQueue
{
	typedef queue<T, deque<T>> queue_t;

	queue_t q;
	bool is_completed;
	int n_producers;
	uint32_t n_elements;

	mutable mutex mtx;								// The mutex to synchronise on
	condition_variable cv_queue_empty;

public:
	// *****************************************************************************************
	//
	CRegisteringQueue(int _n_producers)
	{
		Restart(_n_producers);
	};

	// *****************************************************************************************
	//
	~CRegisteringQueue()
	{};

	// *****************************************************************************************
	//
	void Restart(int _n_producers)
	{
		unique_lock<mutex> lck(mtx);

		is_completed = false;
		n_producers  = _n_producers;
		n_elements = 0;
	}

	// *****************************************************************************************
	//
	bool IsEmpty()
	{
		lock_guard<mutex> lck(mtx);
		return n_elements == 0;
	}

	// *****************************************************************************************
	//
	bool IsCompleted()
	{
		lock_guard<mutex> lck(mtx);

		return n_elements == 0 && n_producers == 0;
	}

	// *****************************************************************************************
	//
	void MarkCompleted()
	{
		lock_guard<mutex> lck(mtx);
		n_producers--;

		if(!n_producers)
			cv_queue_empty.notify_all();
	}

	// *****************************************************************************************
	//
	void Push(T data)
	{
		unique_lock<mutex> lck(mtx);
		bool was_empty = n_elements == 0;
		q.push(data);
		++n_elements;

		if(was_empty)
			cv_queue_empty.notify_all();
	}

	// *****************************************************************************************
	//
	void Emplace(T data)
	{
		Push(data);

		// !!! To implement in the future
	}

	// *****************************************************************************************
	//
	void PushRange(vector<T> &data)
	{
		unique_lock<mutex> lck(mtx);
		bool was_empty = n_elements == 0;
		
		for(auto &x : data)
			q.push(x);
		n_elements += data.size();

		if (was_empty)
			cv_queue_empty.notify_all();
	}

	// *****************************************************************************************
	//
	bool Pop(T &data)
	{
		unique_lock<mutex> lck(mtx);
		cv_queue_empty.wait(lck, [this]{return !this->q.empty() || !this->n_producers;}); 

		if(n_elements == 0)
			return false;

		data = q.front();
		q.pop();
		--n_elements;
		if(n_elements == 0)
			cv_queue_empty.notify_all();

		return true;
	}

	// *****************************************************************************************
	//
	uint32_t GetSize()
	{
		return n_elements;
	}
};

// EOF
