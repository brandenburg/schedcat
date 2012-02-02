#ifndef RES_IO_H
#define RES_IO_H

std::ostream& operator<<(std::ostream &os, const RequestBound &rb);
std::ostream& operator<<(std::ostream &os, const TaskInfo &ti);
std::ostream& operator<<(std::ostream &os, const ResourceSharingInfo &rsi);

#endif
