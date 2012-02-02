#!/usr/bin/env python
#
# Copyright (c) 2010,2011,2012 Bjoern B. Brandenburg <bbb [at] cs.unc.edu>
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the copyright holder nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS  PROVIDED BY THE COPYRIGHT HOLDERS  AND CONTRIBUTORS "AS IS"
# AND ANY  EXPRESS OR  IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED  TO, THE
# IMPLIED WARRANTIES  OF MERCHANTABILITY AND  FITNESS FOR A  PARTICULAR PURPOSE
# ARE  DISCLAIMED. IN NO  EVENT SHALL  THE COPYRIGHT  OWNER OR  CONTRIBUTORS BE
# LIABLE  FOR   ANY  DIRECT,  INDIRECT,  INCIDENTAL,   SPECIAL,  EXEMPLARY,  OR
# CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT   NOT  LIMITED  TO,  PROCUREMENT  OF
# SUBSTITUTE  GOODS OR SERVICES;  LOSS OF  USE, DATA,  OR PROFITS;  OR BUSINESS
# INTERRUPTION)  HOWEVER CAUSED  AND ON  ANY  THEORY OF  LIABILITY, WHETHER  IN
# CONTRACT,  STRICT  LIABILITY, OR  TORT  (INCLUDING  NEGLIGENCE OR  OTHERWISE)
# ARISING IN ANY  WAY OUT OF THE USE  OF THIS SOFTWARE, EVEN IF  ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

"""
Bin-packing-inspired assignment heuristics that support items that
change in size (e.g., utilizations of overhead-affected tasks) and
task-assignment with rollback (if adding a task causes a partition
to become unschedulable, then put it elsewhere).

This was originally implemented for (and discussed in):

   A. Bastoni, B. Brandenburg, and J. Anderson, "Is Semi-Partitioned
   Scheduling Practical?", Proceedings of the 23rd Euromicro Conference
   on Real-Time Systems (ECRTS 2011), pp. 125-135. IEEE, July 2011. 

"""

from .binpack import ignore

class BasicBin(object):
    def __init__(self, initial_items=None):
        self.items = [] if initial_items is None else initial_items

    def infeasible_to_fit(self, item):
        """Returns True if item cannot possible fit, so it's not even worth
        trying."""
        return False

    def prepare(self):
        """prepare() is called to setup a 'transaction' that might have to be
        rolled back"""
        pass

    def temporary_assign(self, new_item):
        """try_assign(x) is called to temporarily add x to the bin"""
        self.items.append(new_item)

    def validate(self):
        """validate() is called to test if the last assignment was ok"""
        return True

    def commit(self):
        """Called if validate() returned True"""
        pass

    def rollback(self):
        """Called if validate() returned False"""
        # default: restore old items
        self.items.pop()

    def try_to_add(self, new_item):
        self.prepare()
        self.temporary_assign(new_item)
        did_it_fit = self.validate()
        if did_it_fit:
            self.commit()
        else:
            self.rollback()
        return did_it_fit

    def try_to_add_no_commit(self, new_item):
        """Doesn't commit if it works, but rolls back if it fails."""
        self.prepare()
        self.temporary_assign(new_item)
        did_it_fit = self.validate()
        if not did_it_fit:
            self.rollback()
        return did_it_fit

    def add(self, new_item):
        if not self.try_to_add(new_item):
            raise DidNotFit(new_item) # nope, already full


class Bin(BasicBin):
    def __init__(self, initial_items=None, size=lambda x: x, capacity=1):
        BasicBin.__init__(self, initial_items)
        self.max_capacity = capacity
        # default: let's just pretend that these are all numbers
        self.size  = size

    def capacity(self):
        return self.max_capacity

    def validate(self):
        return sum([self.size(x) for x in self.items]) <= self.capacity()

    def allocated_capacity(self):
        return sum([self.size(x) for x in self.items])

    def spare_capacity(self):
        return self.capacity() - self.allocated_capacity()

    def infeasible_to_fit(self, item):
        return self.size(item) > self.spare_capacity()


class GlobalConstraintBin(Bin):
    """A Bin that also checks a global constraint before accepting an item."""
    def __init__(self, global_validate=lambda: True, *args, **kargs):
        Bin.__init__(self, *args, **kargs)
        self.global_validate = global_validate

    def validate(self):
        return super(GlobalConstraintBin, self).validate() and self.global_validate()


class DuctileItem(object):
    """A ductile item is an item that can change size based on other items
    in the bin. All "virtual methods" must be overriden."""

    def size(self):
        """returns the current size of the item"""
        assert False

    def copy(self):
        """returns a copy of this item; may return self if it never changes"""
        assert False

    def update_size(self, added_item, bin):
        """change the size of the item based on an item that was added to the
        bin"""
        assert False

    def determine_size(self, bin):
        """determine the initial size of the item based on the bin in which it
        will be placed"""
        assert False


class FixedSizeItem(object):
    """A wrapper to add fixed-size items to ductile bins."""
    def __init__(self, item, item_size):
        self.item_size = item_size
        self.item = item

    def size(self):
        return self.item_size

    def copy(self):
        return self

    def update_size(self, added_item, bin):
        pass # size does not change

    def determine_size(self, bin):
        pass # size does not change


class DuctileBin(Bin):
    """ This is a base class for implementing binpacking with ductile items.
    """
    def __init__(self, *args, **kargs):
        Bin.__init__(self, *args, **kargs)
        self.size = self.item_size

    def item_size(self, item):
        return item.size()

    def prepare(self):
        self.saved_items = self.items
        self.items = [obj.copy() for obj in self.items]

    def temporary_assign(self, new_item):
        for obj in self.items:
            obj.update_size(new_item, self)
        new_item.determine_size(self)
        self.items.append(new_item)

    def rollback(self):
        self.items = self.saved_items
        self.saved_items = None


class CheckedBin(Bin):
    """Debug helper: can be wrapped around a bin to validate that the methods
    are called in the right sequence"""
    STABLE    = "stable"
    PREPPED   = "prepped"
    ASSIGNED  = "assigned"
    VALIDATED = "validated"

    def __init__(self, bin):
        self.__bin = bin
        self.__state = CheckedBin.STABLE

    def __getattr__(self, name):
        # everything that we don't have goes to the proxy
        return self.__bin.__dict__[name]

    def prepare(self):
        assert self.__state == CheckedBin.STABLE
        self.__bin.prepare()
        self.__state = CheckedBin.PREPPED

    def temporary_assign(self, new_item):
        assert self.__state == CheckedBin.PREPPED
        self.__bin.temporary_assign(new_item)
        self.__state = CheckedBin.ASSIGNED

    def validate(self):
        assert self.__state == CheckedBin.ASSIGNED
        res = self.__bin.validate()
        self.__state = CheckedBin.VALIDATED
        return res

    def commit(self):
        assert self.__state == CheckedBin.VALIDATED
        self.__bin.commit()
        self.__state = CheckedBin.STABLE

    def rollback(self):
        assert self.__state == CheckedBin.VALIDATED
        self.__bin.rollback()
        self.__state = CheckedBin.STABLE


class Heuristic(object):
    """Base class for bin-packing heuristics."""

    def __init__(self, initial_bins=None, make_bin=None):
        self.bins     = [] if initial_bins is None else initial_bins
        self.make_bin = make_bin
        self.misfits  = []
        self.remaining_items = []

    def select_bins_for_item(self, item):
        return [] # overide with iterator

    def try_to_place_item(self, item):
        for bin in self.select_bins_for_item(item):
            if bin.try_to_add(item):
                return True
        return False

    def binpack(self, items=[], report_misfit=ignore):
        """Binpack items into given finitet number of bins."""
        self.remaining_items.extend(items)

        count = 0
        while self.remaining_items:
            item = self.remaining_items.pop(0)
            if self.try_to_place_item(item):
                # success
                count += 1
            else:
                # Did not fit in any of the bins that we tried.
                # See if we can add a new bin.
                made_space = False
                if self.make_bin:
                    # yes, let's try that
                    self.bins.append(self.make_bin())
                    # try to fit it in an empty bin
                    made_space = self.bins[-1].try_to_add(item)
                if not made_space:
                    # Either can't add bins or item won't fit into
                    # an empty bin by itself.
                    self.misfits.append(item)
                    report_misfit(item)
                else:
                    count += 1
        return count


class NextFit(Heuristic):
    def __init__(self, *args, **kargs):
        Heuristic.__init__(self, *args, **kargs)
        self.cur_index = 0

    def select_bins_for_item(self, item):
        while self.cur_index < len(self.bins):
            b = self.bins[self.cur_index]
            if not b.infeasible_to_fit(item):
                yield b
            # if that didn't work, then try the next
            self.cur_index += 1


class FirstFit(Heuristic):
    def select_bins_for_item(self, item):
        # simply try each bin
        for bin in self.bins:
            if not bin.infeasible_to_fit(item):
                yield bin


class FitBased(Heuristic):
    def try_to_place_item(self, item):
        # assumes bins have a spare_capacity() function

        # first get rid of the hopeless
        candidates = [b for b in self.bins if not b.infeasible_to_fit(item)]

        # try to fit it where possible
        packed = []
        for b in candidates:
            b.prepare()
            b.temporary_assign(item)
            if b.validate():
                packed.append(b)
            else:
                b.rollback()

        # now we have every bin where it fits
        # find the one with the most slack
        remainders = [b.spare_capacity() for b in packed]
        # last item has most slack
        if remainders:
            best = packed[remainders.index(self.select_remainder(remainders))]
            best.commit()
            for b in packed:
                if b != best:
                    b.rollback()
            return True
        else:
            return False


class WorstFit(FitBased):
    def select_remainder(self, remainders):
        return max(remainders)


class BestFit(FitBased):
    def select_remainder(self, remainders):
        return min(remainders)


class CapacityBased(Heuristic):
    def select_bins_for_item(self, item):
        bins = zip(self.bins, [b.spare_capacity() for b in self.bins])
        self.order_bins(bins)
        for (b, _) in bins:
            if not b.infeasible_to_fit(item):
                yield b


class MaxSpareCapacity(CapacityBased):
    """For item types that do not change size when adding a new item to a bin,
    this is the same as worst-fit, but faster. For item types that change, this is
    not necessarily the same as WorstFit
    """
    def order_bins(self, bins):
        bins.sort(key = lambda (b, c): c, reverse=True)


class MinSpareCapacity(CapacityBased):
    """For item types that do not change size when adding a new item to a bin,
    this is the same as best-fit, but faster. For item types that change, this is
    not necessarily the same as BestFit
    """
    def order_bins(self, bins):
        bins.sort(key = lambda (b, c): c, reverse=False)

