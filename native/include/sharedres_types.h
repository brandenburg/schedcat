#ifndef SHAREDRES_TYPES_H
#define SHAREDRES_TYPES_H

#ifndef SWIG
#include <limits.h>
#include <assert.h>
#include <cmath>

#include <vector>
#include <algorithm>

#include "stl-helper.h"
#endif

typedef enum {
	WRITE = 0,
	READ  = 1,
} request_type_t;

class TaskInfo;

class RequestBound
{
private:
	unsigned int resource_id;
	unsigned int num_requests;
	unsigned int request_length;
	const TaskInfo*    task;
	request_type_t request_type;
	unsigned int request_priority;

public:
	RequestBound(unsigned int res_id,
		     unsigned int num,
		     unsigned int length,
		     const TaskInfo* tsk,
		     request_type_t type = WRITE,
		     unsigned int priority = 0)
		: resource_id(res_id),
		  num_requests(num),
		  request_length(length),
		  task(tsk),
		  request_type(type),
		  request_priority(priority)
	{}

	unsigned int get_max_num_requests(unsigned long interval) const;

	unsigned int get_resource_id() const { return resource_id; }
	unsigned int get_num_requests() const { return num_requests; }
	unsigned int get_request_length() const { return request_length; }

	request_type_t get_request_type() const { return request_type; }

	unsigned int get_request_priority() const { return request_priority; }

	bool is_read() const { return get_request_type() == READ; }
	bool is_write() const { return get_request_type() == WRITE; }

	const TaskInfo* get_task() const { return task; }
};

typedef std::vector<RequestBound> Requests;

class TaskInfo
{
private:
	unsigned int  priority;
	unsigned long period;
	unsigned long deadline;
	unsigned long response;
	unsigned int  cluster;
	unsigned int  id;
	unsigned long cost;
	Requests requests;

public:
	// implicit deadline task
	TaskInfo(unsigned long _period,
		 unsigned long _response,
		 unsigned int _cluster,
		 unsigned int _priority,
		 int _id,
		 unsigned long _cost = 0)
		: priority(_priority),
		  period(_period),
		  deadline(_period),
		  response(_response),
		  cluster(_cluster),
		  id(_id),
		  cost(_cost)
	{}

	// arbitrary deadline task
	TaskInfo(unsigned long _period,
		 unsigned long _deadline,
		 unsigned long _response,
		 unsigned int _cluster,
		 unsigned int _priority,
		 int _id,
		 unsigned long _cost = 0)
		: priority(_priority),
		  period(_period),
		  deadline(_deadline),
		  response(_response),
		  cluster(_cluster),
		  id(_id),
		  cost(_cost)
	{}

	void add_request(unsigned int res_id,
			 unsigned int num,
			 unsigned int length,
			 request_type_t type = WRITE,
			 unsigned int priority = 0)
	{
		requests.push_back(RequestBound(res_id, num, length, this, type, priority));
	}

	const Requests& get_requests() const
	{
		return requests;
	}

	unsigned int get_id() const { return id; }

	/* NOTE: 0 == highest priority! */
	unsigned int  get_priority() const { return priority; } // smaller integer <=> higher prio

	unsigned long get_period() const { return period; }
	unsigned long get_deadline() const { return deadline; }
	unsigned long get_response() const { return response; }
	unsigned int  get_cluster() const { return cluster; }
	unsigned long get_cost() const { return cost; }

	unsigned int get_num_arrivals() const
	{
		return get_total_num_requests() + 1; // one for the job release
	}

	unsigned int get_total_num_requests() const
	{
		unsigned int count = 0;
		foreach(requests, it)
			count += it->get_num_requests();
		return count;
	}

	unsigned int get_max_request_length() const
	{
		unsigned int len = 0;
		foreach(requests, it)
			len = std::max(len, it->get_request_length());
		return len;
	}

	unsigned int get_num_requests(unsigned int res_id) const
	{
		foreach(requests, it)
			if (it->get_resource_id() == res_id)
				return it->get_num_requests();
		return 0;
	}

	unsigned int get_request_length(unsigned int res_id) const
	{
		foreach(requests, it)
			if (it->get_resource_id() == res_id)
				return it->get_request_length();
		return 0;
	}

	unsigned int get_max_num_jobs(unsigned long interval) const;

	// uniprocessor fixed-priority scheduling, only valid for local tasks
	unsigned int uni_fp_local_get_max_num_jobs(unsigned long interval) const;

	// Assuming EDF priorities,
	// how many jobs of this tasks with lower priority (= later deadline) than pending_job
	// can exist while pending_job is pending?
	unsigned int edf_get_max_lower_prio_jobs(const TaskInfo& pending_job) const
	{
		unsigned long interval;
		if (pending_job.get_response() + get_deadline() <= pending_job.get_deadline())
			// pending_job completes so quickly / has such a late deadline
			// that any overlapping job of this task has an earlier deadline
			// pending_job
			return 0;
		else
		{
			interval = get_deadline() + pending_job.get_response();
			assert(interval >= pending_job.get_deadline());
			interval -= pending_job.get_deadline();
			return get_max_num_jobs(interval);
		}
	}

	// Assuming fixed priorities, how many jobs of this task have lower priority
	// than pending job? (either all nor none)
	unsigned int fp_get_max_lower_prio_jobs(const TaskInfo& pending_job) const
	{
		// check relative priority
		if (pending_job.get_priority() < get_priority())
			return get_max_num_jobs(pending_job.get_response());
		else
			return 0;
	}

	unsigned int get_max_lower_prio_jobs(const TaskInfo& pending_job, bool using_edf) const
	{
		if (using_edf)
			return edf_get_max_lower_prio_jobs(pending_job);
		else
			return fp_get_max_lower_prio_jobs(pending_job);
	}

	// Bertogna's workload bound
	unsigned long workload_bound(unsigned long interval) const
	{
		unsigned long slack, n;

		if (get_deadline() > get_response())
			slack = get_deadline() - get_response();
		else
			slack = 0;

		n = std::floor(
			(interval + get_deadline() - get_cost() - slack) / get_period());
		return n * get_cost() + std::min(
			get_cost(),
			interval + get_deadline() - get_cost() - slack - n * get_period());
	}
};

typedef std::vector<TaskInfo> TaskInfos;

class ResourceSharingInfo
{
private:
	TaskInfos tasks;

public:
	ResourceSharingInfo(unsigned int num_tasks)
	{
		// Make sure all tasks will fit without re-allocation.
		tasks.reserve(num_tasks);
	}

	const TaskInfos& get_tasks() const
	{
		return tasks;
	}

	void add_task(unsigned long period,
		      unsigned long response,
		      unsigned int cluster  = 0,
		      unsigned int priority = UINT_MAX,
                      unsigned long cost = 0,
		      unsigned long deadline = 0)
	{
		// Avoid re-allocation!
		assert(tasks.size() < tasks.capacity());
		int id = tasks.size();
		if (!deadline)
			deadline = period;
		tasks.push_back(TaskInfo(period, deadline, response, cluster, priority, id, cost));
	}

	void add_request(unsigned int resource_id,
			 unsigned int max_num,
			 unsigned int max_length,
			 unsigned int locking_priority = 0)
	{
		assert(!tasks.empty());

		TaskInfo& last_added = tasks.back();
		last_added.add_request(resource_id, max_num, max_length);
	}

	void add_request_rw(unsigned int resource_id,
			    unsigned int max_num,
			    unsigned int max_length,
			    int type,
			    unsigned int locking_priority = 0)
	{
		assert(!tasks.empty());
		assert(type == WRITE || type == READ);

		TaskInfo& last_added = tasks.back();
		last_added.add_request(resource_id, max_num, max_length, (request_type_t) type, locking_priority);
	}

};


#define NO_CPU (-1)

class ResourceLocality
{
private:
	std::vector<int> mapping;

public:
	void assign_resource(unsigned int res_id, unsigned int processor)
	{
		while (mapping.size() <= res_id)
			mapping.push_back(NO_CPU);

		mapping[res_id] = processor;
	}

	int operator[](unsigned int res_id) const
	{
		if (mapping.size() <= res_id)
			return NO_CPU;
		else
			return mapping[res_id];
	}

};

class ReplicaInfo
{
private:
	std::vector<unsigned int> num_replicas;

public:
	void set_replicas(unsigned int res_id, unsigned int replicas)
	{
		assert(replicas >= 1);

		while (num_replicas.size() <= res_id)
			num_replicas.push_back(1); // default: not replicated

		num_replicas[res_id] = replicas;
	}

	unsigned int operator[](unsigned int res_id) const
	{
		if (num_replicas.size() <= res_id)
			return 1; // default: not replicated
		else
			return num_replicas[res_id];
	}
};

struct Interference
{
	unsigned int  count;
	unsigned long total_length;

	Interference() : count(0), total_length(0) {}
	Interference(unsigned int length) : count(1), total_length(length) {}

	Interference& operator+=(const Interference& other)
	{
		count        += other.count;
		total_length += other.total_length;
		return *this;
	}

	Interference operator+(const Interference& other) const
	{
		Interference result;
		result.count = this->count + other.count;
		result.total_length = this->total_length + other.total_length;
		return result;
	}

	bool operator<(const Interference& other) const
	{
		return total_length < other.total_length ||
			(total_length == other.total_length
			 && count < other.count);
	}
};

class BlockingBounds
{
private:
	std::vector<Interference> blocking;
	std::vector<Interference> request_span;
	std::vector<Interference> arrival;
	std::vector<Interference> remote;
	std::vector<Interference> local;

public:
	BlockingBounds(unsigned int num_tasks)
		: blocking(num_tasks),
		  request_span(num_tasks)
	{}

	BlockingBounds(const ResourceSharingInfo& info)
		:  blocking(info.get_tasks().size()),
		request_span(info.get_tasks().size()),
		arrival(info.get_tasks().size()),
		remote(info.get_tasks().size()),
		local(info.get_tasks().size())
	{}

	const Interference& operator[](unsigned int idx) const
	{
		assert( idx < size() );
		return blocking[idx];
	}

	Interference& operator[](unsigned int idx)
	{
		assert( idx < size() );
		return blocking[idx];
	}

	void raise_request_span(unsigned idx, const Interference& val)
	{
		assert( idx < size() );
		request_span[idx] = std::max(request_span[idx], val);
	}

	const Interference& get_max_request_span(unsigned idx) const
	{
		assert( idx < size() );
		return request_span[idx];
	}

	void raise_blocking_length(unsigned idx, const Interference& val)
	{
		assert( idx < size() );
		blocking[idx] = std::max(blocking[idx], val);
	}

	size_t size() const
	{
		return blocking.size();
	}

	unsigned long get_blocking_term(unsigned int tsk_index) const
	{
		assert( tsk_index < blocking.size() );
		return blocking[tsk_index].total_length;
	}

	unsigned long get_blocking_count(unsigned int tsk_index) const
	{
		assert( tsk_index < blocking.size() );
		return blocking[tsk_index].count;
	}

	unsigned long get_span_term(unsigned int tsk_index) const
	{
		assert( tsk_index < blocking.size() );
		return request_span[tsk_index].total_length;
	}

	unsigned long get_span_count(unsigned int tsk_index) const
	{
		assert( tsk_index < blocking.size() );
		return request_span[tsk_index].count;
	}

	Interference get_raw_remote_blocking(unsigned int tsk_index) const
	{
		assert( tsk_index < local.size() );
		return remote[tsk_index];
	}

	unsigned long get_remote_blocking(unsigned int tsk_index) const
	{
		assert( tsk_index < remote.size() );
		return remote[tsk_index].total_length;
	}

	unsigned long get_remote_count(unsigned int tsk_index) const
	{
		assert( tsk_index < remote.size() );
		return remote[tsk_index].count;
	}

	void set_remote_blocking(unsigned int tsk_index,
				 const Interference& inf)
	{
		assert( tsk_index < remote.size() );
		remote[tsk_index] = inf;
	}

	unsigned long get_local_blocking(unsigned int tsk_index) const
	{
		assert( tsk_index < local.size() );
		return local[tsk_index].total_length;
	}

	unsigned long get_local_count(unsigned int tsk_index) const
	{
		assert( tsk_index < local.size() );
		return local[tsk_index].count;
	}

	void set_local_blocking(unsigned int tsk_index,
				const Interference& inf)
	{
		assert( tsk_index < local.size() );
		local[tsk_index] = inf;
	}

	unsigned long get_arrival_blocking(unsigned int tsk_index) const
	{
		assert( tsk_index < arrival.size() );
		return arrival[tsk_index].total_length;
	}

	void set_arrival_blocking(unsigned int tsk_index,
				  const Interference& inf)
	{
		assert( tsk_index < arrival.size() );
		arrival[tsk_index] = inf;
	}
};

#endif

