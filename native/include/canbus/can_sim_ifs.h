#ifndef CANBUS_SIM_IFS_H
#define CANBUS_SIM_IFS_H

/* Methods invoked by Python through the Swig interface. */

unsigned long get_job_completion_time(CANTaskSet &ts, 
                                      unsigned long end_of_simulation,
                                      unsigned long taskid,
                                      unsigned long priority,
                                      unsigned long seqno);

#endif
