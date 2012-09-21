#ifndef GEL_PL_H
#define GEL_PL_H

#ifndef SWIG
#include <vector>
#endif

class GELPl
{

 private:
    std::vector<unsigned long> bounds;
    int no_cpus;
    const TaskSet& tasks;
    int rounds;
    std::vector<double> S_i;
    std::vector<double> G_i;

    // For faster lookups, to avoid too many conversions.
    std::vector<double> utilizations;

    double compute_exact_s(double S, const std::vector<double>& Y_ints);
    double compute_binsearch_s(double S, const std::vector<double>& Y_ints);

    inline double compute_M(double s, double S,
                                   const std::vector<double>& Y_ints);

    // These are basically just structs that override operator< to allow
    // sort algorithms to work.
    class ReplacementType {
     public:
        unsigned int old_task;
        unsigned int new_task;
        double location;
	double old_task_utilization;

        bool operator<(const ReplacementType& other) const {
            return (location < other.location)
                   || ((location == other.location)
                       && (old_task_utilization < other.old_task_utilization));
        }
    };

    class TaggedValue {
     public:
        unsigned int task;
        double value;

        //Order is reversed - we are going to want the largest, rather than the
        //smallest, values.
        bool operator<(const TaggedValue& other) const {
            return other.value < value;
        }
    };

 public:
   enum Scheduler {
     GEDF,
     GFL
   };

   GELPl(Scheduler sched,
         unsigned int num_processors,
         const TaskSet& tasks,
         unsigned int rounds);

   unsigned long get_bound(unsigned int index) {
        return bounds[index];
   }

   double get_Si(unsigned int index) {
   	return S_i[index];
   }

   double get_Gi(unsigned int index) {
   	return G_i[index];
   }
};

#endif
