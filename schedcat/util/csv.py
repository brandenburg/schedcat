from __future__ import absolute_import

import csv

from .storage import storage

def load_columns(fname,
                 convert=lambda x: x,
                 expect_uniform=True):
    """Load a file of CSV data. The first row is assumed
    to contain column labels. These labels can then be used to
    reference individual columns.
    
    x = load_column_csv(...)
    x.by_name -> columns by name
    x.by_idx  -> columns by index in the file
    x.columns -> all columns
    """
    if isinstance(fname, str):
        f = open(fname)
    else:
        # assume we got a file object
        f = fname
    d = list(csv.reader(f))
    if fname != f:
        f.close()

    # infer column labels
    col_idx = {}
    for i, key in enumerate(d[0]):
        col_idx[key.strip()] = i

    max_idx = i

    data    = d[1:]

    if expect_uniform:
        for row in data:
            if len(row) != max_idx + 1:
                print len(row), max_idx
                msg = "expected uniform row length (%s:%d)" % \
                    (fname, data.index(row) + 1)
                raise IOError, msg # bad row length

    # column iterator
    def col(i):
        for row in data:
            if row:
                yield convert(row[i])

    by_col_name = {}
    by_col_idx  = [0] * (max_idx + 1)

    for key in col_idx:
        by_col_name[key] = list(col(col_idx[key]))
        by_col_idx[col_idx[key]] = by_col_name[key]

    return storage(name=fname, columns=col_idx,
                   by_name=by_col_name, by_idx=by_col_idx)
