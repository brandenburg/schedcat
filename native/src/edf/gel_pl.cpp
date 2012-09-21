#include "tasks.h"

#include "edf/gel_pl.h"

#include <vector>
#include <limits>
#include <algorithm>
#include <cmath>

static bool reversed_order(double first, double second) {
    return second < first;
}

GELPl::GELPl(Scheduler sched, unsigned int num_processors, const TaskSet& ts,
             unsigned int num_rounds)
:no_cpus(num_processors), tasks(ts), rounds(num_rounds)
{
    std::vector<unsigned long> pps;
    double S;
    std::vector<double> Y_ints;

    int task_count = tasks.get_task_count();
    // Reserve capacity in all vectors to minimize allocation costs.
    pps.reserve(task_count);
    Y_ints.reserve(task_count);
    S_i.reserve(task_count);
    G_i.reserve(task_count);

    // For faster lookups
    utilizations.reserve(task_count);
    for (int i = 0; i < task_count; i++) {
        utilizations.push_back(double(tasks[i].get_wcet())
                               / double(tasks[i].get_period()));
    }
    
    unsigned long min_pp = std::numeric_limits<unsigned long>::max();

    // Compute initial priority points, including minimum.
    for (int i = 0; i < task_count; i++) {
        const Task& task = tasks[i];
        unsigned long new_pp = task.get_deadline();
        if (sched == GFL) {
            new_pp -= ((num_processors - 1) * task.get_wcet()) / num_processors;
        }
        pps.push_back(new_pp);
        if (new_pp < min_pp) {
            min_pp = new_pp;
        }
    }

    // Reduce to compute minimum.  Also compute Y intercepts, S_i values, and
    // S.
    S = 0.0;
    for (int i = 0; i < task_count; i++) {
        pps[i] -= min_pp;
        const Task& task = tasks[i];
        double wcet = double(task.get_wcet());
        double period = double(task.get_period());
        S_i[i] = std::max(0.0, wcet * (1.0 -  double(pps[i])/ period));
        S += S_i[i];
        Y_ints.push_back((0.0 - wcet/no_cpus) * (wcet / period)
                         + task.get_wcet() - S_i[i]);
    }

    double s;
    if (rounds == 0) {
        s = compute_exact_s(S, Y_ints);
    }
    else {
        s = compute_binsearch_s(S, Y_ints);
    }

    for (int i = 0; i < task_count; i++) {
        bounds.push_back(pps[i]
                         + tasks[i].get_wcet()
                         + (unsigned long)std::ceil(
                         s - (double(tasks[i].get_wcet() / double(no_cpus)))));
        G_i.push_back(Y_ints[i] + s * utilizations[i]);
    }
}

double GELPl::compute_exact_s(double S, const std::vector<double>& Y_ints) {
    int task_count = tasks.get_task_count();

    std::vector<ReplacementType> replacements;
    for (int i = 0; i < task_count; i++) {
        for (int j = i + 1; j < task_count; j++) {
            // We can ignore parallel and identical lines - either don't
            // intersect or we don't care which is picked.
            if (utilizations[i] != utilizations[j]) {
                double intersect = (Y_ints[j] - Y_ints[i])
                                   / (utilizations[i] - utilizations[j]);
                ReplacementType replacement;
                replacement.location = intersect;
                if (intersect >= 0.0) {
                    if (utilizations[i] < utilizations[j]) {
                        replacement.old_task = i;
			replacement.old_task_utilization = utilizations[i];
                        replacement.new_task = j;
                    }
                    else {
                        replacement.old_task = j;
			replacement.old_task_utilization = utilizations[j];
                        replacement.new_task = i;
                    }
                    replacements.push_back(replacement);
                }
            }
        }
    }
    std::sort(replacements.begin(), replacements.end());

    std::vector<bool> task_pres;
    task_pres.assign(task_count, false);
    
    double current_value = S;
    double current_slope = -1 * no_cpus;

    std::vector<TaggedValue> init_pairs;
    init_pairs.reserve(task_count);
    for (int i = 0; i < task_count; i++) {
    	TaggedValue new_pair;
	new_pair.task = i;
	new_pair.value = Y_ints[i];
        init_pairs.push_back(new_pair);
    }
    
    // Allows us to efficiently compute sum of top m-1 elements.  They may not
    // be in order but must be the correct choices.
    std::nth_element(init_pairs.begin(), init_pairs.begin() + no_cpus - 2,
                     init_pairs.end());

    for (int i = 0; i < no_cpus - 1; i++) {
        unsigned int task_index = init_pairs[i].task;
        task_pres[task_index] = true;
        current_value += init_pairs[i].value;
        current_slope += utilizations[task_index];
    }
    
    unsigned int rindex = 0;
    double next_s = 0.0;
    double zero = std::numeric_limits<double>::infinity();
    while (zero > next_s) {
        double current_s = next_s;
        zero = current_s - current_value / current_slope;
        if (rindex < replacements.size()) {
            ReplacementType replacement = replacements[rindex];
            next_s = replacement.location;
            current_value += (next_s - current_s) * current_slope;
            // Apply replacement, if appropriate
            if (task_pres[replacement.old_task]
                    && !task_pres[replacement.new_task]) {
                task_pres[replacement.old_task] = false;
                current_slope -= utilizations[replacement.old_task];
                task_pres[replacement.new_task] = true;
                current_slope += utilizations[replacement.new_task];
            }
            rindex++;
        }
        else {
            next_s = std::numeric_limits<double>::infinity();
        }
    }
    return zero;
}

double GELPl::compute_binsearch_s(double S, const std::vector<double>& Y_ints) {
    double min_s = 0.0;
    double max_s = 1.0;
    while (compute_M(max_s, S, Y_ints) > 0) {
        min_s = max_s;
        max_s *= 2.0;
    }
    
    for (int i = 0; i < rounds; i++) {
        double middle = (min_s + max_s) / 2.0;
        if (compute_M(middle, S, Y_ints) < 0) {
            max_s = middle;
        }
        else {
            min_s = middle;
        }
    }

    // max_s is guaranteed to be a legal bound.
    return max_s;
}

double GELPl::compute_M(double s, double S, const std::vector<double>& Y_ints) {
    std::vector<double> Gvals;
    int task_count = tasks.get_task_count();
    for (int i = 0; i < task_count; i++) {
        Gvals.push_back(Y_ints[i] + utilizations[i] * s);
    }

    // Again, more efficient computation by not totally sorting.
    std::nth_element(Gvals.begin(), Gvals.begin() + no_cpus - 2, Gvals.end(),
                     reversed_order);
    double to_return = S - no_cpus * s;
    
    for (int i = 0; i < no_cpus - 1; i++) {
        to_return += Gvals[i];
    }

    return to_return;
}
