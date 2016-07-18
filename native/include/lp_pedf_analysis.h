#ifndef LP_PEDF_ANALYSIS_H
#define LP_PEDF_ANALYSIS_H

// ------------------------------------------------------------------
// --------------------[ A N A L Y S I S ]---------------------------
// ------------------------------------------------------------------

// ---------------------[ D E F I N E S ]----------------------------

// Enable debug prints:
// #define __DEBUG_PEDF_BLK_ANALYSIS__

// Enable the stop of the analysis loop at the hyper-period of all
// the tasks in the system:
// #define __PEDF_BLK_ANALYSIS_ENABLE_HP_STOP__

// Enable a timeout for the analysis loop:
// Warning: based on SIGALARM, hence not 100% portable
// #define  __PEDF_BLK_ANALYSIS_ENABLE_TIMEOUT__

// ------------------------------------------------------------------


// Default value used for blocking lower-bound
static unsigned long AVAL = 0;

class PEDFBlockingAnalysis
{
  public:
    PEDFBlockingAnalysis(const ResourceSharingInfo& _info, unsigned int _cluster);

    bool is_schedulable();

  protected:
    virtual unsigned long compute_blocking_PDC(unsigned long interval_length) = 0;
    virtual unsigned long compute_blocking_AC (unsigned long interval_length) = 0;

    virtual unsigned long compute_tighter_blocking_PDC(unsigned long interval_length,
                                                       unsigned long blk_UB,
                                                       unsigned long blk_LB = 0)
    {
        return blk_UB;
    }

    const ResourceSharingInfo& info;
    unsigned int cluster;
    unsigned int max_deadline, min_deadline;

  private:

    //bool processorDemandCriterion(std::map<int, unsigned int>& nJobs, unsigned long maxTime);
    bool QPA(unsigned long t_LB, unsigned long t_UB, unsigned long blk_LB_in = 0, unsigned long& blk_LB_out = AVAL);
    bool raw_PDC(unsigned long t_LB, unsigned long t_UB);
    unsigned long DBF(unsigned long interval_length);
    unsigned long arrival_curve(unsigned long interval_length);
    unsigned long last_check_point_before(unsigned long interval_length);

};

enum analysis_type_t
{
    AC_MODE, // compute LP for an arrival curve
    PDC_MODE // compute processor-demand criterion LP
};

#endif