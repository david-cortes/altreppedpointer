# ALTREP externalptr example

This is a short example of an R package with compiled source code (C++) that uses heap-allocated C++ objects through R's `externalptr` system, managing serialization of these heap-lived objects through the new ALTREP system.

R's class `externalptr` consists of R objects that hold a pointer to an arbitrary memory address, which will typically point to objects of interest used by languages such as C or C++ and which cannot be mapped to R types, such as custom statistical model objects (e.g. from decision-tree libraries) generated when calling native functions through e.g. `.Call` and similar.

Typically, when creating R objects of type `externalptr`, these objects do not survive serialization - i.e. if one saves them with `saveRDS` or `save` and then reads them back with `load` or `readRDS`, the result will be a blank pointer (pointing to the `NULL` or zero address). Hence, one typically needs to have a separate method to create a serialized state for the object that `externalptr` points to, and another de-serialization method which needs to be called **manually** by a user **after** calling `readRDS` or similar. In R packages, this is typically done by immediately creating a serialized state for such objects (which takes extra memory and compute time), bundling that as part of the R object exported from the package, and then calling the de-serialization function when the user tries to use a de-serialized object, which is both inconvenient and inefficient.

The idea with these ALTREP'd objects is to have classes exported from a package which handle serialization automatically for the user - that is, the serialized state is only materialized when calling a function like `save` or `saveRDS`, it is not kept in the object (thus not consuming extra memory), and de-serialization happens automatically when the user calls a function like `readRDS` or `load` (i.e. the `externalptr` points to a de-serialized object, not to the `NULL` address, without the user having to call any additional method).

# How it is achieved

Internally, this is achieved by creating an ALTREP'd R list of length==1, whose only element is an `externalptr` object, but without providing any `DATAPTR` method, only `Elt` and serialization methods.

This ALTREP'd list will contain as the first slot (`data1`) the same `externalptr` object, which is what gets return from `Elt`.

Serialization and de-serialization methods are then provided for this list object, which ensures they get called automatically by R's `saveRDS` / `readRDS` and similar, with these serialization methods handling the C++ object instead of an R `VECSXP` type.

The source code for this example is available under the `src` folder in this repository, where one can see how this workaround mechanism can be triggered.

# Try it out

First install this package:
```r
remotes::install_github("david-cortes/altreppedpointer")
```

This package contains a C++ class (see the C++ source file for details) which is wrapped in R through the following methods:
* `get_custom_object()` : creates a heap-allocated C++ object with its own internal state, returned as the ALTREP'd list explained earlier. By default, this C++ object contains an integer variable which is initialized to zero.
* `check_custom_object(obj)` : prints the internal variable from the C++ heap-allocated object, which is handled through the ALTREP'd object.
* `modify_custom_object(obj, num)` : assigns a new number to the C++ object's internal state (i.e. `check_custom_object` will print this afterwards).
* In order to get the `externalptr` object from R, one can subset the object returned from `get_custom_object`: `obj[[1]]`.
* In order to get the `externalptr` object from the ALTREP'd object in C/C++, one can either subset it like above, or pass the "list" object directly to `.Call` and retrieve it through `R_altrep_data1`.
* One can also inspect the object to see the pointer through `.Internal(inspect(obj))`.


After installing the package, convince yourself that the mechanism works by trying out somethig like this:
```r
library(altreppedpointer)
obj <- get_custom_object()
modify_custom_object(obj, 5)
check_custom_object(obj)

fname <- file.path(tempdir(), "custom_obj.Rds")
saveRDS(obj, fname)
obj2 <- readRDS(fname)

print(obj[[1]])
print(obj2[[1]])

check_custom_object(obj2)
modify_custom_object(obj2, 7)
check_custom_object(obj2)
```

Then restart the R session or save to and load from `.Rdata` if one wishes to, and verify that the pointers are not set to `NULL`, even though no "deserializing" message is printed to the console (gets ommited when it happens during `load`, but is still shown from `readRDS`).

# A few caveats

While this system allows a more efficient way of serializing C++ objects, there are a few bad things that can happen:

* If there is an error during de-serialization when loading `.Rdata` in a new session (for example, when one restarts a session in RStudio), such as "out of memory", this will manifest as objects silently disappearing from the environment without leaving any trace or error message (convince yourself by manually adding calls to `Rf_error(...)` in the C++ de-serialization function).
* Serialization is deferred to the moment that methods like `save` or `saveRDS` are called, which in turn means if an error happens (e.g. "insufficient memory" for the serialized state), it will not be possible to serialize the resulting object, which could potentially lead to losing this data.
* If serialization happens multiple times, it will be slower compared to the more traditional mechanism of keeping the serialization state inside the R object, as each call to a serialization method will involve re-creating the serialized state again.
* Package `qs`, as of version `0.25.6`, is not able to serialize or de-serialize these objects.
