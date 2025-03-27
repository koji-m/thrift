trait RepresentableElement(Representable, CollectionElement):
    ...

fn repr_list[T: RepresentableElement](list: List[T]) -> String:
    var s = String("[")
    for i in range(len(list)):
        s += repr(list[i])
        if i < len(list) - 1:
            s += ", "
    s += "]"
    return s

fn i64_list_eq(list1: List[Int64], list2: List[Int64]) -> Bool:
    if len(list1) != len(list2):
        return False
    for i in range(len(list1)):
        if list1[i] != list2[i]:
            return False
    return True