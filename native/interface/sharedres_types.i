%ignore Interference;
%ignore RequestBound;
%ignore TaskInfo;

%ignore ResourceSharingInfo::get_tasks;

%ignore BlockingBounds::raise_request_span;
%ignore BlockingBounds::get_max_request_span;
%ignore BlockingBounds::operator[](unsigned int);
%ignore BlockingBounds::operator[](unsigned int) const;

%ignore ResourceLocality::operator[](unsigned int) const;
%ignore ReplicaInfo::operator[](unsigned int) const;
