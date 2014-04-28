#ifndef GEDF_H
#define GEDF_H

class GlobalEDF : public SchedulabilityTest
{

 private:
    unsigned int m;
    unsigned long rta_step;
    bool want_ffdbf;
    bool want_load;
    bool want_baruah;
    bool want_rta;

 public:
 GlobalEDF(unsigned int num_processors,
           unsigned long rta_min_step = 1,
           bool want_baruah = true,
           bool want_rta    = true,
           bool want_ffdbf  = false,
           bool want_load   = false)
     : m(num_processors), rta_step(rta_min_step),
       want_ffdbf(want_ffdbf),
       want_load(want_load),
       want_baruah(want_baruah),
       want_rta(want_rta) {};

    bool is_schedulable(const TaskSet &ts, bool check_preconditions = true);
};


#endif
