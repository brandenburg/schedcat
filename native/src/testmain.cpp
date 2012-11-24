#include <iostream>

#include "tasks.h"
#include "task_io.h"
#include "schedulability.h"

#include "sharedres.h"
#include "res_io.h"

#include "edf/baker.h"
#include "edf/baruah.h"
#include "edf/gfb.h"
#include "edf/bcl.h"
#include "edf/bcl_iterative.h"
#include "edf/gedf.h"
#include "edf/sim.h"

#include "event.h"
#include "schedule_sim.h"

#include "math-helper.h"

using namespace std;

void test_baker()
{
    TaskSet ts = TaskSet();

    ts.add_task(49, 100);
    ts.add_task(49, 100);
    ts.add_task(2, 100, 50);

    BakerGedf t = BakerGedf(2);
    cout << "Baker schedulable?  : " << t.is_schedulable(ts) << endl;

    GFBGedf gfb = GFBGedf(2);
    cout << "GFB schedulable?    : " << gfb.is_schedulable(ts) << endl;

    cout << "BCL schedulable?    : " << BCLGedf(2).is_schedulable(ts) << endl;

    cout << "Baruah schedulable? : " << BaruahGedf(2).is_schedulable(ts)
         << endl;
    cout << "BCL Iter. sched.?   : " << BCLIterativeGedf(2).is_schedulable(ts)
         << endl;

    cout << "G-EDF schedulable?  : " << GlobalEDF(2).is_schedulable(ts) << endl;
}

TaskSet* init_baruah()
{
    TaskSet* rts = new TaskSet();
    TaskSet& ts = *rts;

    ts.add_task(544, 89000);
    ts.add_task(7038, 96000);
    ts.add_task(8213, 91000);
    ts.add_task(2937, 39000);
    ts.add_task(3674, 51000);
    ts.add_task(3758, 97000);
    ts.add_task(91, 31000);
    ts.add_task(4960, 55000);
    ts.add_task(3888, 89000);
    ts.add_task(1187, 32000);
    ts.add_task(2393, 44000);
    ts.add_task(1513, 17000);
    ts.add_task(2264, 38000);
    ts.add_task(6660, 84000);
    ts.add_task(1183, 96000);
    ts.add_task(4810, 95000);
    ts.add_task(1641, 20000);
    ts.add_task(3968, 71000);
    ts.add_task(280, 82000);
    ts.add_task(4259, 51000);
    ts.add_task(1981, 70000);
    ts.add_task(393, 34000);
    ts.add_task(3882, 93000);
    ts.add_task(5921, 68000);
    ts.add_task(901, 21000);
    ts.add_task(2166, 40000);
    ts.add_task(1532, 17000);
    ts.add_task(1159, 36000);
    ts.add_task(2170, 89000);
    ts.add_task(8770, 91000);
    ts.add_task(1643, 48000);
    ts.add_task(110, 69000);
    ts.add_task(1300, 84000);
    ts.add_task(1488, 20000);
    ts.add_task(2031, 21000);
    ts.add_task(7139, 95000);
    ts.add_task(3905, 63000);
    ts.add_task(8126, 82000);
    ts.add_task(6309, 82000);
    ts.add_task(7386, 80000);
    ts.add_task(5044, 83000);
    ts.add_task(425, 77000);
    ts.add_task(1439, 38000);
    ts.add_task(6332, 74000);
    ts.add_task(1237, 62000);
    ts.add_task(2547, 32000);
    ts.add_task(1196, 12000);
    ts.add_task(9996, 100000);
    ts.add_task(2730, 31000);
    ts.add_task(773, 48000);
    ts.add_task(3894, 59000);
    ts.add_task(1234, 39000);
    ts.add_task(1585, 34000);
    ts.add_task(1905, 67000);
    ts.add_task(3440, 62000);
    ts.add_task(678, 24000);
    ts.add_task(7211, 97000);
    ts.add_task(1453, 60000);
    ts.add_task(6560, 84000);
    ts.add_task(122, 73000);
    ts.add_task(382, 42000);
    ts.add_task(2906, 46000);
    ts.add_task(880, 11000);
    ts.add_task(2704, 29000);
    ts.add_task(2387, 36000);
    ts.add_task(3111, 46000);
    ts.add_task(4654, 78000);
    ts.add_task(808, 81000);
    ts.add_task(1485, 18000);
    ts.add_task(1865, 73000);
    ts.add_task(2956, 64000);
    ts.add_task(1058, 51000);
    ts.add_task(1773, 86000);
    ts.add_task(2610, 54000);
    ts.add_task(6795, 86000);
    ts.add_task(8381, 84000);
    ts.add_task(5631, 85000);
    ts.add_task(1567, 69000);
    ts.add_task(303, 24000);
    ts.add_task(2889, 44000);
    ts.add_task(4201, 67000);
    ts.add_task(2771, 85000);
    ts.add_task(1287, 71000);
    ts.add_task(4572, 67000);
    ts.add_task(4277, 54000);
    ts.add_task(3114, 82000);
    ts.add_task(4527, 49000);
    ts.add_task(2336, 60000);
    ts.add_task(8131, 85000);
    ts.add_task(2680, 27000);
    ts.add_task(2598, 34000);
    ts.add_task(888, 58000);
    ts.add_task(1051, 14000);
    ts.add_task(1216, 27000);
    ts.add_task(2768, 40000);
    ts.add_task(875, 65000);
    ts.add_task(3762, 49000);
    ts.add_task(5294, 56000);
    ts.add_task(6273, 97000);
    ts.add_task(7594, 91000);
    ts.add_task(2948, 83000);
    ts.add_task(1315, 16000);
    ts.add_task(4982, 79000);
    ts.add_task(127, 10000);
    ts.add_task(372, 11000);
    ts.add_task(4487, 59000);
    ts.add_task(1388, 46000);
    ts.add_task(3443, 40000);
    ts.add_task(221, 32000);
    ts.add_task(1121, 12000);
    ts.add_task(872, 32000);
    ts.add_task(1540, 43000);
    ts.add_task(2794, 43000);
    ts.add_task(4840, 68000);
    ts.add_task(40, 21000);
    ts.add_task(710, 44000);
    ts.add_task(253, 37000);
    ts.add_task(611, 14000);
    ts.add_task(2646, 48000);
    ts.add_task(3239, 64000);
    ts.add_task(413, 22000);
    ts.add_task(1451, 35000);
    ts.add_task(444, 29000);
    ts.add_task(119, 38000);
    ts.add_task(873, 15000);
    ts.add_task(688, 20000);
    ts.add_task(5667, 88000);
    ts.add_task(1226, 34000);
    ts.add_task(1743, 25000);
    ts.add_task(1732, 25000);
    ts.add_task(359, 77000);
    ts.add_task(8101, 86000);
    ts.add_task(1909, 44000);
    ts.add_task(2326, 33000);
    ts.add_task(148, 70000);
    ts.add_task(764, 26000);
    ts.add_task(1951, 26000);
    ts.add_task(430, 33000);
    ts.add_task(430, 24000);
    ts.add_task(3216, 69000);
    ts.add_task(6476, 71000);
    ts.add_task(1728, 88000);
    ts.add_task(517, 92000);
    ts.add_task(6755, 69000);
    ts.add_task(737, 17000);
    ts.add_task(1480, 68000);
    ts.add_task(2392, 53000);
    ts.add_task(795, 12000);
    ts.add_task(1676, 31000);
    ts.add_task(4412, 80000);
    ts.add_task(2937, 53000);
    ts.add_task(2129, 76000);
    ts.add_task(1413, 34000);
    ts.add_task(214, 10000);
    ts.add_task(1844, 50000);
    ts.add_task(2612, 31000);
    ts.add_task(4326, 65000);
    ts.add_task(7053, 98000);
    ts.add_task(2952, 83000);
    ts.add_task(507, 68000);
    ts.add_task(1112, 51000);
    ts.add_task(110, 89000);
    ts.add_task(1468, 17000);
    ts.add_task(7788, 83000);
    ts.add_task(688, 16000);
    ts.add_task(2195, 48000);
    ts.add_task(1636, 61000);
    ts.add_task(530, 19000);
    ts.add_task(3543, 45000);
    ts.add_task(2023, 24000);
    ts.add_task(3818, 55000);
    ts.add_task(2032, 65000);
    ts.add_task(1790, 63000);
    ts.add_task(69, 12000);
    ts.add_task(1569, 90000);
    ts.add_task(8860, 98000);
    ts.add_task(2330, 64000);
    ts.add_task(971, 35000);
    ts.add_task(2168, 87000);
    ts.add_task(2309, 56000);
    ts.add_task(752, 14000);
    ts.add_task(4573, 81000);
    ts.add_task(1015, 99000);
    ts.add_task(4131, 60000);
    ts.add_task(1324, 50000);
    ts.add_task(2354, 68000);
    ts.add_task(4137, 86000);
    ts.add_task(2671, 36000);
    ts.add_task(3642, 50000);
    ts.add_task(3017, 33000);
    ts.add_task(567, 15000);
    ts.add_task(3310, 45000);
    ts.add_task(1727, 23000);
    ts.add_task(9067, 100000);
    ts.add_task(324, 11000);
    ts.add_task(2299, 62000);
    ts.add_task(645, 28000);
    ts.add_task(7903, 91000);
    ts.add_task(843, 22000);
    ts.add_task(5727, 80000);
    ts.add_task(5308, 75000);
    ts.add_task(574, 11000);
    ts.add_task(497, 30000);
    ts.add_task(7536, 91000);
    ts.add_task(540, 92000);
    ts.add_task(233, 12000);
    ts.add_task(2253, 29000);
    ts.add_task(1298, 84000);
    ts.add_task(1516, 84000);
    ts.add_task(2292, 57000);
    ts.add_task(2216, 25000);
    ts.add_task(2496, 43000);
    ts.add_task(4050, 47000);
    ts.add_task(480, 17000);
    ts.add_task(941, 27000);
    ts.add_task(9024, 91000);
    ts.add_task(1318, 29000);
    ts.add_task(2862, 56000);
    ts.add_task(3194, 61000);
    ts.add_task(614, 15000);
    ts.add_task(3039, 92000);
    ts.add_task(4494, 58000);
    ts.add_task(814, 11000);
    ts.add_task(9271, 97000);
    ts.add_task(569, 62000);
    ts.add_task(3625, 84000);
    ts.add_task(2095, 23000);
    ts.add_task(3789, 95000);
    ts.add_task(4866, 78000);
    ts.add_task(3109, 96000);
    ts.add_task(2659, 42000);
    ts.add_task(1427, 44000);
    ts.add_task(3311, 55000);
    ts.add_task(651, 26000);
    ts.add_task(1254, 52000);
    ts.add_task(3250, 91000);
    ts.add_task(2073, 92000);
    ts.add_task(6143, 90000);
    ts.add_task(7444, 85000);
    ts.add_task(7359, 87000);
    ts.add_task(350, 51000);
    ts.add_task(5597, 70000);
    ts.add_task(5278, 77000);
    ts.add_task(3116, 72000);
    ts.add_task(4043, 51000);
    ts.add_task(4912, 59000);
    ts.add_task(8909, 90000);
    ts.add_task(755, 48000);
    ts.add_task(348, 10000);
    ts.add_task(3065, 88000);
    ts.add_task(4136, 49000);
    ts.add_task(8198, 82000);
    ts.add_task(4925, 91000);
    ts.add_task(779, 10000);
    ts.add_task(1134, 12000);
    ts.add_task(3999, 46000);
    ts.add_task(1687, 38000);
    ts.add_task(565, 22000);
    ts.add_task(1553, 56000);
    ts.add_task(8208, 89000);
    ts.add_task(2237, 31000);
    ts.add_task(6885, 90000);
    ts.add_task(664, 16000);
    ts.add_task(549, 17000);
    ts.add_task(3799, 50000);
    ts.add_task(3707, 52000);
    ts.add_task(896, 27000);
    ts.add_task(1897, 74000);
    ts.add_task(1528, 25000);
    ts.add_task(4931, 55000);
    ts.add_task(1882, 95000);
    ts.add_task(3642, 96000);
    ts.add_task(2586, 57000);
    ts.add_task(2432, 31000);
    ts.add_task(1036, 24000);
    ts.add_task(4127, 45000);
    ts.add_task(7284, 84000);
    ts.add_task(2020, 57000);
    ts.add_task(901, 10000);
    ts.add_task(2017, 21000);
    ts.add_task(4991, 52000);
    ts.add_task(3064, 63000);
    ts.add_task(1369, 23000);
    ts.add_task(5174, 67000);
    ts.add_task(1023, 26000);
    ts.add_task(629, 54000);
    ts.add_task(1164, 22000);
    ts.add_task(3074, 38000);
    ts.add_task(2285, 72000);
    ts.add_task(2190, 53000);
    ts.add_task(681, 33000);
    ts.add_task(3818, 66000);
    ts.add_task(1926, 41000);
    ts.add_task(5677, 73000);
    ts.add_task(1132, 16000);
    ts.add_task(930, 27000);
    ts.add_task(2323, 63000);
    ts.add_task(635, 13000);
    ts.add_task(1328, 57000);
    ts.add_task(2107, 28000);
    ts.add_task(1174, 39000);
    ts.add_task(190, 70000);
    ts.add_task(1437, 15000);
    ts.add_task(6367, 82000);
    ts.add_task(323, 80000);
    ts.add_task(1230, 13000);
    ts.add_task(1603, 88000);
    ts.add_task(367, 24000);
    ts.add_task(3227, 48000);
    ts.add_task(7160, 73000);
    ts.add_task(136, 12000);
    ts.add_task(2582, 77000);
    ts.add_task(145, 45000);
    ts.add_task(6384, 79000);
    ts.add_task(1013, 63000);
    ts.add_task(7001, 88000);
    ts.add_task(1525, 27000);
    ts.add_task(3928, 78000);
    ts.add_task(734, 62000);
    ts.add_task(953, 43000);
    ts.add_task(3062, 77000);
    ts.add_task(740, 15000);
    ts.add_task(3978, 53000);
    ts.add_task(1113, 55000);
    ts.add_task(2475, 94000);
    ts.add_task(3168, 34000);
    ts.add_task(236, 40000);
    ts.add_task(148, 39000);
    ts.add_task(2814, 53000);
    ts.add_task(5107, 64000);
    ts.add_task(5425, 78000);
    ts.add_task(320, 14000);
    ts.add_task(6885, 99000);
    ts.add_task(4699, 61000);
    ts.add_task(5917, 77000);
    ts.add_task(7350, 80000);
    ts.add_task(2231, 29000);
    ts.add_task(4231, 79000);
    ts.add_task(4007, 86000);
    ts.add_task(198, 53000);
    ts.add_task(7140, 72000);
    ts.add_task(217, 43000);
    ts.add_task(309, 41000);
    ts.add_task(212, 18000);
    ts.add_task(1167, 24000);
    ts.add_task(5243, 58000);
    ts.add_task(1623, 63000);
    ts.add_task(242, 28000);
    ts.add_task(293, 74000);
    ts.add_task(6670, 96000);
    ts.add_task(2009, 41000);
    ts.add_task(887, 24000);
    ts.add_task(615, 16000);
    ts.add_task(1493, 51000);
    ts.add_task(5020, 53000);
    ts.add_task(6192, 81000);
    ts.add_task(4928, 63000);
    ts.add_task(3958, 60000);
    ts.add_task(3479, 56000);
    ts.add_task(1470, 75000);
    ts.add_task(1020, 17000);
    ts.add_task(4903, 56000);
    ts.add_task(7938, 86000);
    ts.add_task(871, 11000);
    ts.add_task(7242, 95000);
    ts.add_task(845, 40000);
    ts.add_task(2646, 33000);
    ts.add_task(4409, 51000);
    ts.add_task(736, 32000);
    ts.add_task(691, 14000);
    ts.add_task(328, 100000);
    ts.add_task(8384, 91000);
    ts.add_task(536, 50000);
    ts.add_task(180, 40000);
    ts.add_task(6117, 89000);
    ts.add_task(913, 37000);
    ts.add_task(4403, 70000);
    ts.add_task(6350, 78000);
    ts.add_task(419, 60000);
    ts.add_task(3469, 91000);
    ts.add_task(296, 23000);
    ts.add_task(2256, 24000);
    ts.add_task(1588, 90000);
    ts.add_task(2659, 100000);
    ts.add_task(1759, 18000);
    ts.add_task(4062, 93000);
    ts.add_task(1216, 14000);
    ts.add_task(162, 32000);
    ts.add_task(1643, 68000);
    ts.add_task(2409, 46000);
    ts.add_task(1522, 28000);
    ts.add_task(840, 30000);
    ts.add_task(2491, 41000);
    ts.add_task(2712, 96000);
    ts.add_task(3297, 100000);
    ts.add_task(6269, 96000);
    ts.add_task(2319, 93000);
    ts.add_task(973, 55000);
    ts.add_task(3753, 68000);
    ts.add_task(1449, 36000);
    ts.add_task(1293, 17000);
    ts.add_task(1991, 37000);
    ts.add_task(958, 13000);
    ts.add_task(3343, 61000);
    ts.add_task(493, 82000);
    ts.add_task(1555, 51000);
    ts.add_task(3194, 92000);
    ts.add_task(1594, 18000);
    ts.add_task(650, 33000);
    ts.add_task(5761, 63000);
    ts.add_task(3998, 98000);
    ts.add_task(5874, 100000);
    ts.add_task(2371, 47000);
    ts.add_task(1771, 74000);
    ts.add_task(983, 22000);
    ts.add_task(2026, 73000);
    ts.add_task(3573, 54000);
    ts.add_task(939, 18000);
    ts.add_task(3585, 60000);
    ts.add_task(2480, 43000);
    ts.add_task(3534, 54000);
    ts.add_task(7482, 80000);
    ts.add_task(57, 17000);
    ts.add_task(1342, 86000);
    ts.add_task(2339, 33000);
    ts.add_task(675, 61000);

    return rts;
}

void test_baruah()
{
    TaskSet *ts =     init_baruah();

    cout << "Baruah schedulable? : " << BaruahGedf(24).is_schedulable(*ts)
         << endl;
}

int bar_main(int argc, char** argv)
{
    test_baruah();
    return 0;
}

int xxxmain(int argc, char** argv)
{
    cout << "GMP C++ test." << endl;

    integral_t a, b;

    a = "123123123123";
    b = "456456456456";

    cout << "a     : " << a << endl;
    cout << "b     : " << b << endl;
    cout << "a*b*10: " << a * b * 10 << endl;

    fractional_t q = a;

    q /= b;
    cout << "a/b   :" << q << endl;

    integral_t fact;
    fact = 1;
    for (int n = 2; n < 101; n++) {
	fact *= n;
    }
    cout << "Factorial is " << fact << endl;
    cout << "casted: " << fact.get_ui() << endl;

    Task t = Task(10, 100);

    cout << "wcet: " << t.get_wcet() << " period: " << t.get_period()
	 << " deadline: " << t.get_deadline() << endl;


    fractional_t lambda, bound;
    unsigned int m = 10;

    lambda = 3;
    lambda /= 10;
    bound = m * (1 - lambda) + lambda;

    cout << "lambda: " << lambda << " bound: " << bound << endl;

    test_baker();

    return 0;
}


template <typename JobPriority>
class  DebugGlobalScheduler : public GlobalScheduler<JobPriority>
{
  public:
    DebugGlobalScheduler(int m) : GlobalScheduler<JobPriority>(m) {};

    void at_time()
    {
        cout << "at time " << this->get_current_time() << ": ";
    }

    virtual void job_released(Job *job)
    {
        at_time();
        cout << "released job " << job->get_seqno() << " of " << job->get_task()
             << endl;
    };

    virtual void job_completed(int proc,
                               Job *job)
    {
        at_time();
        cout << "completed job " << job->get_seqno() << " of " << job->get_task();
        if (job->get_deadline() < this->get_current_time())
            cout << " TARDINESS: " << this->get_current_time() - job->get_deadline();
        cout << endl;
    };

    virtual void job_scheduled(int proc,
                               Job *preempted,
                               Job *scheduled)
    {
        at_time();
        cout << "scheduled job " << scheduled->get_seqno() << " of "
             << scheduled->get_task() << " on CPU " << proc;
        if (preempted)
            cout << "; preempted job " << preempted->get_seqno() << " of "
                 << preempted->get_task();
        else
            cout << " [CPU was idle] ";
        cout   << endl;
    };
};

#define NUM_TASKS 3

int xmain(int argc, char** argv)
{
    DebugGlobalScheduler<EarliestDeadlineFirst> theSim(2);

    TaskSet ts = TaskSet();

    /*    ts[0].init(10, 100);
    ts[1].init(3, 9);
    ts[2].init(11, 33);
    ts[3].init(11, 17);
    ts[4].init(2, 5);
    */

    ts.add_task(20, 30);
    ts.add_task(20, 30);
    ts.add_task(20, 30);

    PeriodicJobSequence* gen[NUM_TASKS];
    for (int i = 0; i < NUM_TASKS; i++) {
        gen[i] = new PeriodicJobSequence(ts[i]);
        gen[i]->set_simulation(&theSim);
        theSim.add_release(gen[i]);
    }

    theSim.simulate_until(1000);

    return 0;
}


int xxxxmain(int argc, char** argv)
{
    GlobalScheduler<EarliestDeadlineFirst> theSim(24);

    TaskSet* ts = init_baruah();

    PeriodicJobSequence** gen;
    gen = new PeriodicJobSequence*[ts->get_task_count()];

    for (unsigned int i = 0; i < ts->get_task_count(); i++) {
        gen[i] = new PeriodicJobSequence((*ts)[i]);
        gen[i]->set_simulation(&theSim);
        theSim.add_release(gen[i]);
    }

    theSim.simulate_until(1000 * 1000 * 1000); // 1000 seconds

    return 0;
}


int yymain(int argc, char** argv)
{
    TaskSet* ts = init_baruah();
    simtime_t end = 10 * 60 * 1000 * 1000; // 10 minutes
    for (int m = 1; m < 30; m++)
        cout << "\nOn " << m << " CPUs "
             << "deadline missed: " << edf_misses_deadline(m, *ts, end) << endl;
    return 0;
}


int main4(int argc, char** argv)
{
    fractional_t a, b;
    integral_t c;

    a = 20;
    a /= 3;
    cout << a << endl;
    //    b = a % 3;
    b = a / 3;
    cout << b << endl;

    c = b; // truncate

    cout << c << endl;

    truncate_fraction(b);
    cout << b << endl;

    return 0;
}


int main5(int argc, char** argv)
{
	unsigned long a, b;

	a = 133;
	b = 10;

	cout << a << " // " <<  b << " = " << divide_with_ceil(a, b) << endl;

	a = 130;

	cout << a << " // " <<  b << " = " << divide_with_ceil(a, b) << endl;

	a = 129;

	cout << a << " // " <<  b << " = " << divide_with_ceil(a, b) << endl;

	return 0;
}

/*
int main6(int argc, char** argv)
{
	RequestSourceSet rset(10);
	unsigned int i;

	for (i = 0; i < rset.get_source_count(); i++)
		rset[i].init(1, 10 + i, 100 * (i + 1), 0, i);

	rset.sort();

	for (i = 0; i < rset.get_source_count(); i++)
		cout << "pos " << i << " " << rset[i]
		     << " -> " << rset[i].get_max_num_requests(700) << endl;


	cout << "blocking: " << rset.bound_blocking(700, 0, 4, 0) << endl;

	return 0;
}



*/



int main6(int argc, char** argv)
{
	TaskInfo ti(100, 100, 0, 0);

	ti.add_request(123, 3, 3);
	ti.add_request(103, 1, 2);

	cout << "task: " << ti << endl;

	return 0;
}


int main7(int argc, char** argv)
{
	ResourceSharingInfo rsi(3);
	unsigned int i;

	rsi.add_task(50, 50);
	rsi.add_request(0, 2, 1);

	rsi.add_task(30, 30);
	rsi.add_request(0, 1, 3);

	rsi.add_task(20, 20);
	rsi.add_request(0, 1, 1);

	cout << rsi << endl;

	//	cout << "Global OMLP: " << rset.global_omlp_bound(20, 1, 16, 2) << endl;

	BlockingBounds* results;

	results = global_omlp_bounds(rsi, 16);

	for (i = 0; i < 3; i++)
		cout << i << ": count=" << (*results)[i].count
		     << " total=" << (*results)[i].total_length << endl;

	return 0;
}


int main(int argc, char** argv)
{
	ResourceSharingInfo rsi(100);
	unsigned int i;

	rsi.add_task(50000, 50000, 0, 2);
	rsi.add_request(0, 2, 1);

	rsi.add_task(30000, 30000, 0, 1);
	rsi.add_request(0, 4, 3);

	rsi.add_task(20000, 20000, 0, 0);
	rsi.add_request(0, 4, 1);


	rsi.add_task(50000, 50000, 1, 3);
	rsi.add_request(0, 2, 1);

	rsi.add_task(30000, 30000, 1, 2);
	rsi.add_request(0, 3, 3);
	rsi.add_request(1, 100, 100);

	rsi.add_task(20000, 20000, 1, 1);
	rsi.add_request(0, 3, 1);

	rsi.add_task(50000, 50000, 2, 2);
	rsi.add_request(0, 2, 1);

	rsi.add_task(30000, 30000, 2, 1);
	rsi.add_request(0, 5, 3);

	rsi.add_task(20000, 20000, 2, 0);
	rsi.add_request(0, 2, 1);

	for (i = 0; i < 30; i++)
	{
		rsi.add_task(100000 + i, 100000 + i, 0, 3 + i);
		rsi.add_request(0, 1, 1);
		rsi.add_request(3, 1, 1);
	}

	rsi.add_task(3000, 3000, 3, 0);
	rsi.add_request(1, 1, 1);

	rsi.add_task(5000, 5000, 1, 0);

	rsi.add_task(100000, 100000, 4, 100);
	rsi.add_request(3, 3, 3);

	cout << rsi << endl;

	BlockingBounds* results;

	results = global_omlp_bounds(rsi, 6);

	cout << endl << endl  << "Global OMLP" << endl;
	for (i = 0; i < results->size(); i++)
		cout << i << ": count=" << (*results)[i].count
		     << " total=" << (*results)[i].total_length << endl;

	delete results;


	results = global_fmlp_bounds(rsi);

	cout << endl << endl  << "Global FMLP" << endl;
	for (i = 0; i < results->size(); i++)
		cout << i << ": count=" << (*results)[i].count
		     << " total=" << (*results)[i].total_length << endl;

	delete results;

	results = part_omlp_bounds(rsi);

	cout << endl << endl  << "Partitioned OMLP" << endl;
	for (i = 0; i < results->size(); i++)
		cout << i
		     << ": count=" << (*results)[i].count
		     << " total=" << (*results)[i].total_length
		     << "  --- request span: count=" << results->get_span_count(i)
		     << "  total=" << results->get_span_term(i)
		     << endl;

	delete results;

	results = clustered_omlp_bounds(rsi, 1);

	cout << endl << endl  << "Clustered OMLP c=1" << endl;
	for (i = 0; i < results->size(); i++)
		cout << i
		     << ": count=" << (*results)[i].count
		     << " total=" << (*results)[i].total_length
		     << "  --- request span: count=" << results->get_span_count(i)
		     << "  total=" << results->get_span_term(i)
		     << endl;

	delete results;

	results = clustered_omlp_bounds(rsi, 3);

	cout << endl << endl  << "Clustered OMLP c=3" << endl;
	for (i = 0; i < results->size(); i++)
		cout << i
		     << ": count=" << (*results)[i].count
		     << " total=" << (*results)[i].total_length
		     << "  --- request span: count=" << results->get_span_count(i)
		     << "  total=" << results->get_span_term(i)
		     << endl;

	delete results;

	results = part_fmlp_bounds(rsi);

	cout << endl << endl  << "Part FMLP" << endl;
	for (i = 0; i < results->size(); i++)
		cout << i
		     << ": count=" << (*results)[i].count
		     << " total=" << (*results)[i].total_length
		     << endl;

	delete results;

	results = mpcp_bounds(rsi, false);

	cout << endl << endl  << "MPCP::susp" << endl;
	for (i = 0; i < results->size(); i++)
		cout << i
		     << ": total=" << (*results)[i].total_length
		     << "  remote=" << results->get_span_term(i)
		     << endl;

	delete results;

	results = mpcp_bounds(rsi, true);

	cout << endl << endl  << "MPCP::spin" << endl;
	for (i = 0; i < results->size(); i++)
		cout << i
		     << ": total=" << (*results)[i].total_length
		     << "  remote=" << results->get_span_term(i)
		     << endl;

	delete results;
	return 0;
}
