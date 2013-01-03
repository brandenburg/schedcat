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
    int util_ceil;
    int rounds;
    std::vector<fractional_t> S_i;
    std::vector<fractional_t> G_i;

    // For faster lookups, to avoid too many conversions.
    std::vector<fractional_t> utilizations;

    void compute_exact_s(const fractional_t& S,
                         const std::vector<fractional_t>& Y_ints,
                         fractional_t& s);
    void compute_binsearch_s(const fractional_t& S,
                             const std::vector<fractional_t>& Y_ints,
                             fractional_t& s);

    inline bool M_lt_0(const fractional_t& s, const fractional_t& S,
                       const std::vector<fractional_t>& Y_ints);

    // These are basically just structs that override operator< to allow
    // sort algorithms to work.
    class ReplacementType {
     public:
        unsigned int old_task;
        unsigned int new_task;
        fractional_t location;
	fractional_t old_task_utilization;

        bool operator<(const ReplacementType& other) const {
            return (location < other.location)
                   || ((location == other.location)
                       && (old_task_utilization < other.old_task_utilization));
        }
    };

    class TaggedValue {
     public:
        unsigned int task;
        fractional_t value;

        //Order is reversed - we are going to want the largest, rather than the
        //smallest, values.
        bool operator<(const TaggedValue& other) const {
            return other.value < value;
        }
    };

 public:

   GELPl(unsigned int num_processors,
         const TaskSet& tasks,
         unsigned int rounds);

   unsigned long get_bound(unsigned int index) {
        return bounds[index];
   }

   // Converted to double for the sake of Python
   double get_Si(unsigned int index) {
   	return S_i[index].get_d();
   }

   // Converted to double for the sake of Python
   double get_Gi(unsigned int index) {
   	return G_i[index].get_d();
   }
};

#endif
