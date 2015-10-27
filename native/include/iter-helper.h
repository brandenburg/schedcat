#ifndef ITER_HELPER_H
#define ITER_HELPER_H

// A generic for loop that iterates 'request_index_variable' from 0 to the
// maximum number of requests issued by task tx while ti is pending. 'tx_request'
// should be of type RequestBound&.
#define foreach_request_instance(tx_request, task_ti, request_index_variable) \
	for (								\
		unsigned int __max_num_requests = (tx_request).get_max_num_requests((task_ti).get_response()), \
				 request_index_variable = 0;		\
		request_index_variable < __max_num_requests;		\
		request_index_variable++				\
		)

// iterate over each task using 'task_iter', skipping 'excluded_task'
#define foreach_task_except(tasks, excluded_task, task_iter)	\
	foreach(tasks, task_iter)				\
	if (task_iter->get_id() != (excluded_task).get_id())

// iterate only over tasks with equal or lower priority
#define foreach_lowereq_priority_task(tasks, reference_task, task_iter) \
	foreach(tasks, task_iter)				      \
	if (task_iter->get_priority() >= (reference_task).get_priority())

// iterate only over tasks with equal or lower priority, excluding 'reference_task'
#define foreach_lowereq_priority_task_except(tasks, reference_task, task_iter) \
	foreach(tasks, task_iter)				      \
	if (task_iter->get_priority() >= (reference_task).get_priority() &&    \
		task_iter->get_id() != (reference_task).get_id())

// iterate only over tasks with higher priority than 'reference task', excluding 'excluded_task'
#define foreach_higher_priority_task_except(tasks, reference_task, excluded_task, task_iter) \
	foreach(tasks, task_iter)				      \
	if (task_iter->get_priority() < (reference_task).get_priority() &&    \
		task_iter->get_id() != (excluded_task).get_id())

// iterate only over tasks with higher priority
#define foreach_higher_priority_task(tasks, reference_task, task_iter) \
	foreach(tasks, task_iter)				       \
	if (task_iter->get_priority() < (reference_task).get_priority())

// iterate only over tasks with lower priority
#define foreach_lower_priority_task(tasks, reference_task, task_iter) \
	foreach(tasks, task_iter)				       \
	if (task_iter->get_priority() > (reference_task).get_priority())

// iterate over requests not in the local cluster
#define foreach_remote_request(requests, locality, task_ti, request_iter) \
	foreach(requests, request_iter)					\
	if ((locality)[request_iter->get_resource_id()]			\
	    != (int) (task_ti).get_cluster())

// iterate over requests for resources in a specific cluster
#define foreach_request_in_cluster(requests, locality, cluster, request_iter) \
	foreach(requests, request_iter)					\
	if ((locality)[request_iter->get_resource_id()]			\
	    == (int) (cluster))

// iterate over each task using 'task_iter', skipping tasks in the same
// cluster as 'local_task'
#define foreach_remote_task(tasks, local_task, task_iter)	\
	foreach(tasks, task_iter)				\
	if (task_iter->get_cluster() != (local_task).get_cluster())

// iterate only over tasks with equal or lower priority, excluding local tasks
#define foreach_remote_lowereq_priority_task(tasks, reference_task, task_iter) \
	foreach_remote_task(tasks, reference_task, task_iter)			   \
	if (task_iter->get_priority() >= (reference_task).get_priority())

// iterate only over tasks with higher priority, excluding local tasks
#define foreach_remote_higher_priority_task(tasks, reference_task, task_iter) \
	foreach_remote_task(tasks, reference_task, task_iter)		   \
	if (task_iter->get_priority() < (reference_task).get_priority())

#define foreach_task_in_cluster(tasks, cluster, task_iter) \
	foreach(tasks, task_iter) \
	if (task_iter->get_cluster() == (cluster))

#define foreach_task_not_in_cluster(tasks, cluster, task_iter) \
	foreach(tasks, task_iter) \
	if (task_iter->get_cluster() != (cluster))

#define foreach_local_task(tasks, local_task, task_iter)	\
	foreach(tasks, task_iter)				\
	if (task_iter->get_cluster() == (local_task).get_cluster())

#define foreach_local_task_except(tasks, local_task, task_iter)	\
	foreach(tasks, task_iter)				\
	if (task_iter->get_cluster() == (local_task).get_cluster() && \
	    task_iter->get_id() != (local_task).get_id())

#define foreach_local_lowereq_priority_task_except(tasks, local_task, task_iter)	\
	foreach(tasks, task_iter)				\
	if (task_iter->get_cluster() == (local_task).get_cluster() && \
	    task_iter->get_id() != (local_task).get_id() && \
	    task_iter->get_priority() >= (local_task).get_priority())

#define foreach_request_for(requests, res_id, req_iter)	\
	foreach(requests, req_iter)				\
	if (req_iter->get_resource_id() == res_id)


#endif
