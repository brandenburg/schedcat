#ifndef _GLOBAL_PIP_H_
#define _GLOBAL_PIP_H_

unsigned long Ilp_i(
	const ResourceSharingInfo& info,
	const TaskInfo &tsk,
	unsigned int number_of_cpus);

unsigned long lower_priority_with_higher_ceiling_time(
	const ResourceSharingInfo& info,
	const TaskInfo &tsk,
	const TaskInfo &tx,
	const PriorityCeilings &prio_ceilings);

unsigned long common_sr_time(
	const ResourceSharingInfo& info,
	const TaskInfo* tsk,
	const TaskInfo &tx);

unsigned long Ihp_i_dsr(
	const ResourceSharingInfo& info,
	const TaskInfo* tsk);

unsigned long W_l_tx(
	const ResourceSharingInfo& info,
	unsigned long t,
	const TaskInfo &task,
	unsigned long x);

unsigned long DB_i(
	const ResourceSharingInfo& info,
	const TaskInfo &tsk);

#endif
