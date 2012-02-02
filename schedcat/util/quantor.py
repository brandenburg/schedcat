
def forall(lst):
    def predicate(p):
        for x in lst:
            if not p(x):
                return False
        return True
    return predicate

def exists(lst):
    def predicate(p):
        for x in lst:
            if p(x):
                return True
        return False
    return predicate
