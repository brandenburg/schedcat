#include "tasks.h"
#include "schedulability.h"

#include "edf/baker.h"
#include "edf/baruah.h"
#include "edf/gfb.h"
#include "edf/bcl.h"
#include "edf/bcl_iterative.h"
#include "edf/rta.h"
#include "edf/ffdbf.h"
#include "edf/load.h"
#include "edf/la.h"
#include "edf/gedf.h"

bool GlobalEDF::is_schedulable(const TaskSet &ts,
                               bool check)
{
    if (check)
	{
        if (!(ts.has_only_feasible_tasks() && ts.is_not_overutilized(m)))
            return false;

        if (ts.get_task_count() == 0)
            return true;
    }

    if (!ts.has_no_self_suspending_tasks())
        return want_la && LAGedf(m).is_schedulable(ts, false);

    // density bound on a uniprocessor.
    if (m == 1)
    {
        fractional_t density;
        ts.get_density(density);
        if (density <= 1)
            return true;
    }

    // Baker's test can deal with arbitrary deadlines.
    // It's cheap, so do it first.
    if (BakerGedf(m).is_schedulable(ts, false))
        return true;

    // Baruah's test and the BCL and GFB tests assume constrained deadlines.
    if (ts.has_only_constrained_deadlines())
	    if (GFBGedf(m).is_schedulable(ts, false)
            || (want_rta && RTAGedf(m, rta_step).is_schedulable(ts, false))
               // The RTA test generalizes the BCL and BCLIterative tests.
            || (want_baruah && BaruahGedf(m).is_schedulable(ts, false))
            || (want_ffdbf && FFDBFGedf(m).is_schedulable(ts, false)))
            return true;

    // LA test can handle arbitrary deadlines
    if (want_la && LAGedf(m).is_schedulable(ts, false))
        return true;

    // Load-based test can handle arbitrary deadlines.
    if (want_load && LoadGedf(m).is_schedulable(ts, false))
        return true;

    return false;
}
