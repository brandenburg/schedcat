#include "tasks.h"

#include "edf/gel_pl.h"

#include <vector>
#include <limits>
#include <algorithm>
#include <cmath>
#include <iostream>

static bool reversed_order(const fractional_t& first,
                           const fractional_t& second) {
    return second < first;
}

GELPl::GELPl(unsigned int num_processors, const TaskSet& ts,
             unsigned int num_rounds)
:no_cpus(num_processors), tasks(ts), rounds(num_rounds)
{
    fractional_t sys_utilization;
    tasks.get_utilization(sys_utilization);
    // Compute ceiling
    integral_t util_ceil_pre = sys_utilization.get_num();
    mpz_cdiv_q(util_ceil_pre.get_mpz_t(),
               sys_utilization.get_num().get_mpz_t(),
               sys_utilization.get_den().get_mpz_t());
    util_ceil = util_ceil_pre.get_ui();
    std::vector<unsigned long> prio_pts;
    fractional_t S = 0;
    std::vector<fractional_t> Y_ints;

    int task_count = tasks.get_task_count();
    // Reserve capacity in all vectors to minimize allocation costs.
    prio_pts.reserve(task_count);
    Y_ints.reserve(task_count);
    S_i.reserve(task_count);
    G_i.reserve(task_count);

    // For faster lookups
    utilizations.reserve(task_count);
    for (int i = 0; i < task_count; i++) {
        utilizations.push_back(tasks[i].get_wcet());
        utilizations[i] /= tasks[i].get_period();
    }
    
    unsigned long min_prio_pt = std::numeric_limits<unsigned long>::max();

    // Compute initial priority points, including minimum.
    for (int i = 0; i < task_count; i++) {
        const Task& task = tasks[i];
        unsigned long new_prio_pt = task.get_prio_pt();
        prio_pts.push_back(new_prio_pt);
        if (new_prio_pt < min_prio_pt) {
            min_prio_pt = new_prio_pt;
        }
    }

    // Reduce to compute minimum.  Also compute Y intercepts, S_i values, and
    // S.
    for (int i = 0; i < task_count; i++) {
        prio_pts[i] -= min_prio_pt;
        const Task& task = tasks[i];
        unsigned long wcet = task.get_wcet();
        unsigned long period = task.get_period();
        S_i.push_back(prio_pts[i]);
        fractional_t& S_i_i = S_i[i];
        S_i_i *= -1;
        S_i_i /= period;
        S_i_i += 1;
        S_i_i *= wcet;
        if (S_i_i < 0) {
            S_i_i = 0;
        }
        S += S_i_i;
        Y_ints.push_back(wcet);
        fractional_t& Y_ints_i = Y_ints[i];
	Y_ints_i *= -1;
        Y_ints_i /= no_cpus;
        Y_ints_i *= utilizations[i];
        Y_ints_i += wcet;
        Y_ints_i -= S_i_i;
    }

    fractional_t s;
    if (rounds == 0) {
        compute_exact_s(S, Y_ints, s);
    }
    else {
        compute_binsearch_s(S, Y_ints, s);
    }

    for (int i = 0; i < task_count; i++) {
        fractional_t x_i = s;
        fractional_t x_comp = tasks[i].get_wcet();
        x_comp /= no_cpus;
        x_i -= x_comp;
        // Compute ceiling
        integral_t xi_ceil = x_i.get_num();
        mpz_cdiv_q(xi_ceil.get_mpz_t(),
                   x_i.get_num().get_mpz_t(),
                   x_i.get_den().get_mpz_t());
        bounds.push_back(prio_pts[i]
                         + tasks[i].get_wcet()
                         + xi_ceil.get_ui());
        G_i.push_back(s);
        G_i[i] *= utilizations[i];
        G_i[i] += Y_ints[i];
    }
}

void GELPl::compute_exact_s(const fractional_t& S,
                            const std::vector<fractional_t>& Y_ints,
                            fractional_t& s) {
    int task_count = tasks.get_task_count();

    std::vector<ReplacementType> replacements;
    for (int i = 0; i < task_count; i++) {
        for (int j = i + 1; j < task_count; j++) {
            // We can ignore parallel and identical lines - either don't
            // intersect or we don't care which is picked.
            if (utilizations[i] != utilizations[j]) {
                fractional_t intersect_den = utilizations[i];
                intersect_den -= utilizations[j];
                fractional_t intersect = Y_ints[j];
                intersect -= Y_ints[i];
                intersect /= intersect_den;
                ReplacementType replacement;
                replacement.location = intersect;
                if (intersect >= 0) {
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
    
    fractional_t current_value = S;
    fractional_t current_slope = no_cpus;
    current_slope *= -1;

    std::vector<TaggedValue> init_pairs;
    init_pairs.reserve(task_count);
    for (int i = 0; i < task_count; i++) {
    	TaggedValue new_pair;
        new_pair.task = i;
        new_pair.value = Y_ints[i];
        init_pairs.push_back(new_pair);
    }
    
    // Only if we have tasks contributing to G
    if (util_ceil >= 2) {
        // Allows us to efficiently compute sum of top m-1 elements.  They may
        // not be in order but must be the correct choices.
        std::nth_element(init_pairs.begin(),
                         init_pairs.begin() + util_ceil - 2,
                         init_pairs.end());

        for (int i = 0; i < util_ceil - 1; i++) {
            unsigned int task_index = init_pairs[i].task;
            task_pres[task_index] = true;
            current_value += init_pairs[i].value;
            current_slope += utilizations[task_index];
        }
    }
    
    unsigned int rindex = 0;
    fractional_t next_s = 0;
    s = 1;
    while (s > next_s) {
        fractional_t current_s = next_s;
        s = current_value;
        s /= current_slope;
        s *= -1;
        s += current_s;
        if (rindex < replacements.size()) {
            ReplacementType replacement = replacements[rindex];
            next_s = replacement.location;
            fractional_t val_inc = next_s;
            val_inc -= current_s;
            val_inc *= current_slope;
            current_value += val_inc;
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
            next_s = s;
            next_s += 1;
        }
    }
    // At this point, "s" should be the appropriate return value
}

void GELPl::compute_binsearch_s(const fractional_t& S,
                                const std::vector<fractional_t>& Y_ints,
				fractional_t& s) {
    fractional_t min_s = 0;
    fractional_t max_s = 1;
    while (!M_lt_0(max_s, S, Y_ints)) {
        min_s = max_s;
        max_s *= 2;
    }
    
    for (int i = 0; i < rounds; i++) {
        fractional_t middle = min_s;
	middle += max_s;
	middle /= 2;
        if (M_lt_0(middle, S, Y_ints)) {
            max_s = middle;
        }
        else {
            min_s = middle;
        }
    }

    // max_s is guaranteed to be a legal bound.
    s = max_s;
}

bool GELPl::M_lt_0(const fractional_t& s, const fractional_t& S,
                   const std::vector<fractional_t>& Y_ints) {
    std::vector<fractional_t> Gvals;
    int task_count = tasks.get_task_count();
    for (int i = 0; i < task_count; i++) {
        Gvals.push_back(utilizations[i]);
        Gvals[i] *= s;
        Gvals[i] += Y_ints[i];
    }

    fractional_t final_val = no_cpus;
    final_val *= -1;
    final_val *= s;
    final_val += S;
    
    // Only if there will be tasks contributing to G
    if (util_ceil >= 2) {
        // Again, more efficient computation by not totally sorting.
        std::nth_element(Gvals.begin(),
                         Gvals.begin() + util_ceil - 2,
                         Gvals.end(),
                         reversed_order);
        for (int i = 0; i < util_ceil - 1; i++) {
            final_val += Gvals[i];
        }
    }

    return (final_val < 0);
}
