#include <R.h>
#include <Rinternals.h>
#include <R_ext/Altrep.h>

#include <cstring>
#include <memory>

class CustomCppClass
{
public:
    CustomCppClass() {
        Rprintf("Object is being created anew\n");
    }

    ~CustomCppClass() {
        Rprintf("Object is being destructed\n");
    }

    void check_number() const {
        Rprintf("internal number is: %d\n", this->number);
    }

    void modify_number(int new_number) {
        Rprintf("Object is being modified\n");
        this->number = new_number;
    }

    R_xlen_t get_serialized_size() const {
        return sizeof(int);
    }

    void serialize_to_char(unsigned char *state) const {
        Rprintf("Object is being serialized\n");
        std::memcpy(state, &this->number, sizeof(int));
    }

    /* to de-serialized from saved state */
    CustomCppClass(const unsigned char *state) {
        Rprintf("Object is being de-serialized\n");
        std::memcpy(&this->number, state, sizeof(int));
    }

private:
    int number = 0;
};

extern "C" {

static R_altrep_class_t altrepped_pointer_class;

void delete_cpp_object(SEXP R_ptr)
{
    CustomCppClass *ptr_to_cpp_obj = (CustomCppClass*)R_ExternalPtrAddr(R_ptr);
    delete ptr_to_cpp_obj;
}

/* Generate an auto-serializable ALTREP'd version of the C++ class.

   Structure is as follows:
   - Slot 1: R 'externalptr' object to the address of the heap-allocated C++ object
   - Slot 2: NULL */
SEXP create_alrepped_cpp_object()
{
    CustomCppClass *ptr_to_cpp_obj = new CustomCppClass();
    SEXP R_ptr = PROTECT(R_MakeExternalPtr(ptr_to_cpp_obj, R_NilValue, R_NilValue));
    R_RegisterCFinalizerEx(R_ptr, delete_cpp_object, TRUE);

    SEXP R_altrepped_obj = PROTECT(R_new_altrep(altrepped_pointer_class, R_ptr, R_NilValue));
    UNPROTECT(2);
    return R_altrepped_obj;
}

/* This function serializes the heap-allocated C++ object to a 'raw' vector,
   from which the object can then be re-constructed. It will be assigned as
   serialization method for the altrepped object, so it will be triggered
   automatically when calling 'save' or 'saveRDS'. */
SEXP generate_serialized_state(SEXP R_altrepped_obj)
{
    SEXP R_ptr = R_altrep_data1(R_altrepped_obj);
    CustomCppClass *ptr_to_cpp_obj = (CustomCppClass*)R_ExternalPtrAddr(R_ptr);
    
    R_xlen_t state_size = ptr_to_cpp_obj->get_serialized_size();
    SEXP R_state = PROTECT(Rf_allocVector(RAWSXP, state_size));
    ptr_to_cpp_obj->serialize_to_char(RAW(R_state));
    
    UNPROTECT(1);
    return R_state;
}

/* This function re-creates the C++ object from the serialized state, returning the
   altrepped R object after reconstructing it.

   It should get called automatically when executing 'load' or 'readRDS', whereas
   for a regular 'externalptr', it would have to be triggered manually.

   Note that the first argument is not used. */
SEXP deserialize_altrepped_object(SEXP cls, SEXP R_state)
{
    std::unique_ptr<CustomCppClass> new_cpp_obj(new CustomCppClass(RAW(R_state)));
    SEXP R_ptr = PROTECT(R_MakeExternalPtr(new_cpp_obj.get(), R_NilValue, R_NilValue));
    R_RegisterCFinalizerEx(R_ptr, delete_cpp_object, TRUE);
    new_cpp_obj.release();

    SEXP R_altrepped_obj = PROTECT(R_new_altrep(altrepped_pointer_class, R_ptr, R_NilValue));
    UNPROTECT(2);
    return R_altrepped_obj;
}

/* In order to avoid getting strange errors when inspecting the altrepped
   objects with methods like 'str', they will be represented in the R side
   as a 'list' object with a single element which consists of an 'externalptr'
   (the object held in the first slot) which contains a pointer to the C++
   object that's held in the first slot.

   This is not strictly necessary, but otherwise one might see strange
   things from calling 'str' on the object. */
R_xlen_t altrepped_object_length(SEXP R_altrepped_obj)
{
    return 1;
}

SEXP get_element_from_altrepped_obj(SEXP R_altrepped_obj, R_xlen_t idx)
{
    return R_altrep_data1(R_altrepped_obj);
}

/* If one wants to be able to assign a different class to the resulting object,
   a duplication method is also required. If one doesn't want to modify the
   'class' attribute, this will not be required */
SEXP duplicate_altrepped_object(SEXP R_altrepped_obj, Rboolean deep)
{
    if (!deep) {
        Rprintf("Object is being duplicated (shallow)\n");
        return R_new_altrep(altrepped_pointer_class, R_altrep_data1(R_altrepped_obj), R_NilValue);
    }

    else {
        Rprintf("Object is being duplicated (deep)\n");
        const CustomCppClass *old_cpp_obj_ptr = (const CustomCppClass*)R_ExternalPtrAddr(R_altrep_data1(R_altrepped_obj));
        std::unique_ptr<CustomCppClass> new_cpp_obj{};
        *new_cpp_obj = *old_cpp_obj_ptr;
        SEXP R_ptr = PROTECT(R_MakeExternalPtr(new_cpp_obj.get(), R_NilValue, R_NilValue));
        R_RegisterCFinalizerEx(R_ptr, delete_cpp_object, TRUE);
        new_cpp_obj.release();

        SEXP R_altrepped_obj = PROTECT(R_new_altrep(altrepped_pointer_class, R_ptr, R_NilValue));
        UNPROTECT(2);
        return R_altrepped_obj;
    }
}

/* https://purrple.cat/blog/2018/10/14/altrep-and-cpp/ */
Rboolean altrepped_object_inspect(SEXP x, int pre, int deep, int pvec, void (*inspect_subtree)(SEXP, int, int, int))
{
    SEXP R_ptr = R_altrep_data1(x);
    Rprintf(
        "Altrepped external pointer [address:%p]\n",
        R_ExternalPtrAddr(R_ptr)
    );
    return TRUE;
}

/* A couple functions to interact with the object */
SEXP object_check_number(SEXP R_altrepped_obj)
{
    SEXP R_ptr = R_altrep_data1(R_altrepped_obj);
    CustomCppClass *ptr_to_cpp_obj = (CustomCppClass*)R_ExternalPtrAddr(R_ptr);
    ptr_to_cpp_obj->check_number();
    return R_NilValue;
}

SEXP object_modify_number(SEXP R_altrepped_obj, SEXP new_number)
{
    SEXP R_ptr = R_altrep_data1(R_altrepped_obj);
    CustomCppClass *ptr_to_cpp_obj = (CustomCppClass*)R_ExternalPtrAddr(R_ptr);
    ptr_to_cpp_obj->modify_number(Rf_asInteger(new_number));
    return R_NilValue;
}

/* Registering the functions */
static const R_CallMethodDef callMethods [] = {
    {"object_check_number", (DL_FUNC) &object_check_number, 1},
    {"object_modify_number", (DL_FUNC) &object_modify_number, 2},
    {"create_alrepped_cpp_object", (DL_FUNC) &create_alrepped_cpp_object, 0},
    {NULL, NULL, 0}
};

void R_init_altreppedpointer(DllInfo *info)
{
    R_registerRoutines(info, NULL, callMethods, NULL, NULL);
    R_useDynamicSymbols(info, TRUE);

    /* Here the altrepped object needs to register its methods.
       Note that the second argument is the package name. */
    altrepped_pointer_class = R_make_altlist_class("altrepped_pointer_class", "altreppedpointer", info);
    R_set_altrep_Length_method(altrepped_pointer_class, altrepped_object_length);
    R_set_altrep_Inspect_method(altrepped_pointer_class, altrepped_object_inspect);
    R_set_altrep_Serialized_state_method(altrepped_pointer_class, generate_serialized_state);
    R_set_altrep_Unserialize_method(altrepped_pointer_class, deserialize_altrepped_object);
    R_set_altrep_Duplicate_method(altrepped_pointer_class, duplicate_altrepped_object);
    R_set_altlist_Elt_method(altrepped_pointer_class, get_element_from_altrepped_obj);
}

} /* extern "C" */
