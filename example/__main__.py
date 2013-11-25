#Necessary includes and stuff

from example.driver import nolock_example, lock_example, \
                           generate_random_nolock_sets, \
                           generate_random_lock_sets, print_bounds

if __name__ == '__main__':
    #Actually run examples when this script is executed
    print "Running non-lock example"
    print_bounds(nolock_example(generate_random_nolock_sets()))
    print "Running lock example"
    print_bounds(lock_example(generate_random_lock_sets()))
