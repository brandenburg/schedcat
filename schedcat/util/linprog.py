from StringIO import StringIO
from collections import defaultdict


try:
    import cplex
    from itertools import izip
    from tempfile import NamedTemporaryFile as TmpFile

    cplex_available = True
except ImportError:
    cplex_available = False

# Constraint format: tuple of ( vector, value),
# where vector is a list of (coefficient, variable_name) pairs.

def write_cplex_terms(file, vector):
    for (c, v) in vector:
        if c < 0:
            file.write("- %s %s " %  (-c, v))
        else:
            file.write("+ %s %s " %  (c, v))

def write_cplex_sum(file, sum, per_line=None):
    if len(sum) == 0:
        file.write("0")
        return
    file.write("%s %s " %  sum[0])
    if per_line is None:
        write_cplex_terms(file, sum[1:])
    else:
        write_cplex_terms(file, sum[1:per_line])
        for i in xrange(per_line, len(sum), per_line):
            file.write('\n  ')
            write_cplex_terms(file, sum[i:i+per_line])

def filter_vars(removed, sum):
    return [(c, v) for (c, v) in sum if not v in removed]

def apply_prefix(vector, prefix):
    "Prepend prefix to each variable name in vector."
    return [(coeff, prefix + '_' + var) for (coeff, var) in vector]


class Solution(defaultdict):
    def __init__(self):
        super(Solution, self).__init__(int)

    def evaluate(self, vector):
        value = 0
        for (coeff, variable) in vector:
            if variable in self:
                value += coeff * self[variable]
        return value

    def __call__(self, vector):
        return self.evaluate(vector)


class LinearProgram(object):

    def __init__(self):
        self.equalities = []
        self.inequalities = []
        self.objective_function = None
        self.goal = 'Maximize'
        self.name = 'OPT'

    ## Data model

    def add_inequality(self, vector, upper_bound):
        self.inequalities.append((vector, upper_bound))

    def add_equality(self, vector, value):
        self.equalities.append((vector, value))

    def set_objective(self, objective_function):
        self.objective_function = objective_function

    def apply_variable_prefix(self, prefix):
        self.inequalities = [(apply_prefix(vector, prefix), upper_bound)
                             for (vector, upper_bound) in self.inequalities]
        self.equalities = [(apply_prefix(vector, prefix), bound)
                             for (vector, bound) in self.equalities]
        self.objective_function = apply_prefix(self.objective_function, prefix)

    def merge(self, other_LP):
        assert(other_LP.goal == self.goal)
        for ineq in other_LP.inequalities:
            self.inequalities.append(ineq)
        for eq in other_LP.equalities:
            self.equalities.append(eq)
        if self.objective_function == None:
            self.objective_function = []
        for term in other_LP.objective_function:
            self.objective_function.append(term)

    ## Convenience constraint DSL

    def equality(self, *args, **kargs):
        """Specify equality constraint as:

            equality(coeff1, var1, coeff2, var2, ..., equal_to=value)
        """
        assert len(args) % 2 == 0 # num coefficients == num variables?
        assert len(kargs) == 1 and 'equal_to' in kargs # constraint?

        vector = zip(args[0::2], args[1::2])
        self.add_equality(vector, kargs['equal_to'])

    def inequality(self, *args, **kargs):
        """Specify inequality constraint as:

            inequality(coeff1, var1, coeff2, var2, ..., at_most=value)
        """
        assert len(args) % 2 == 0 # num coefficients == num variables?
        assert len(kargs) == 1 and 'at_most' in kargs # constraint?

        vector = zip(args[0::2], args[1::2])
        self.add_inequality(vector, kargs['at_most'])

    def objective(self, *args):
        """Specify objective function as:

            objective(coeff1, var1, coeff2, var2, ...)
        """
        assert len(args) % 2 == 0 # num coefficients == num variables?

        vector = zip(args[0::2], args[1::2])
        self.set_objective(vector)

    ## Helper for model conversion

    def write_cplex_lp_format(self, file):
        file.write('%s\n' % self.goal)
        write_cplex_sum(file, self.objective_function, per_line=7)
        file.write('\nSubject To\n')
        for vector, value in self.equalities:
            write_cplex_sum(file, vector, per_line=7)
            file.write(' = %f\n' % value)
        for vector, value in self.inequalities:
            write_cplex_sum(file, vector, per_line=7)
            file.write(' <= %f\n' % value)
        file.write('End\n')
        file.flush()


    def kill_non_positive_vars(self):
        """Removes all variables that are forced to <= 0.
        Assumes that all coefficients are positive.
        """
        # find pointless variables
        killed = set()
        for vector, value in self.equalities:
            if value <= 0:
                for coeff, var in vector:
                    killed.add(var)
        for vector, value in self.inequalities:
            if value <= 0:
                for coeff, var in vector:
                    killed.add(var)
        # remove pointless variables
        self.objective_function  = filter_vars(killed, self.objective_function)
        if 'local_objective' in self.__dict__:
            self.local_objective  = filter_vars(killed, self.local_objective)
        self.equalities = [(filter_vars(killed, vect), bound) for
                           (vect, bound) in self.equalities if bound > 0]
        self.equalities = [(vect, bound) for (vect, bound) in self.equalities if vect]
        self.inequalities = [(filter_vars(killed, vect), bound) for
                             (vect, bound) in self.inequalities if bound > 0]
        self.inequalities = [(vect, bound) for (vect, bound) in self.inequalities if vect]

    def __str__(self):
        f = StringIO()
        self.write_cplex_lp_format(f)
        return f.getvalue()

    ## Conditional solver integration.

    if cplex_available:

        def get_cplex_model(self, model=None):
            if model is None:
                model = cplex.Cplex()

            # load problem via file to avoid API overheads
            tmp = TmpFile()
            self.write_cplex_lp_format(tmp)
            model.read(tmp.name)
            model.set_results_stream(None)
            del tmp
            return model

        def solve_with_cplex(self):
            solution = Solution()

            # If the objective is empty (e.g., after killing
            # all zero-constrained variables), then we automatically have
            # a trivial solution.
            if len(self.objective_function) == 0:
                return solution

            # Let CPLEX do the job.
            model = self.get_cplex_model()
            model.solve()

            # Extract the variables that we care about.
            variables = [var for (coeff, var) in self.objective_function]
            values = model.solution.get_values(variables)
            for (var, val) in izip(variables, values):
                solution[var] = val

            return solution

    def solve(self):
        if cplex_available:
            return self.solve_with_cplex()
        # Add additional solvers here...
        else:
            assert False # No solver available!

def example():
    #  simple example of the convenience API
    x  = LinearProgram()
    x.objective(-3, 'x2', 99, 'x1')
    x.inequality(10, 'x1', at_most=100)
    x.inequality(-9, 'x1', 10, 'x2', at_most=99)
    x.equality(10, 'x2', equal_to=0)
    print x

    x.inequality(0, 'x3', at_most=1)
    x.equality(0, 'x10', equal_to=0)

if __name__ == '__main__':
    example()
