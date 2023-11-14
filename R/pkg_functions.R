get_custom_object <- function() {
    return(.Call(create_alrepped_cpp_object))
}

check_custom_object <- function(obj) {
    return(.Call(object_check_number, obj))
}

modify_custom_object <- function(obj, newval) {
    return(.Call(object_modify_number, obj, newval))
}
