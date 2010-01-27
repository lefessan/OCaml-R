/* Wrapping and unwrapping of R values. */

/* TODO: Abstract some low level wrapping to a dedicated .so (i.e. other project). */

/* TODO: Think of replacing caml_alloc by caml_alloc_small for efficiency
   reasons. However, one has to deal with GC interaction. See the posts
   http://www.mail-archive.com/caml-list@yquem.inria.fr/msg00725.html and
   http://www.mail-archive.com/caml-list@yquem.inria.fr/msg00726.html */

/* TODO: There are some uses of malloc that should be replaced by
   caml_stat_alloc, which is malloc, but raises the OCaml out of memory
   exception. See http://old.nabble.com/help-with-caml_stat_alloc-tc5463112.html */

/* TODO: To wrap strings, (which we do elsewhere), or more precisely, to
   convert an OCaml string to a C string, INRIA guys use the construct

          char * p;
          p = caml_stat_alloc(caml_string_length(path) + 1);
          strcpy(p, String_val(path));
          [...]
          caml_stat_free(p);

   We should investigate this construct. */

/* TODO: We are now using custom blocks for pointer to R SEXPs. We use values of
   0, 1 in caml_alloc_custom to calibrate the GC. This is obviously insufficient,
   as a SEXP might hold a huge amount of data, specifically in R. We should
   therefore think of writing a BigVal_sexp(SEXP sexp, mlsize_t mem, mlsize_t max)
   function, allowing us to taylor the behaviour of the GC, depending on data size. */

/* TODO: Unfortunately, the scheme used to protect and unprotect values in R with
   ReleaseObject and PreserveObject is highly inefficient. The time to release an
   object is linear in the amount of object preserved. This is a huge runtime penalty
   to interface OCaml's garbage collector with R's garbage collector. Therefore, the
   only way forward is to create our own garbage collector for the interface within
   a global generic vector. Easy, or not? */

/* TODO: When executing an R function call, we let go off the OCaml multithreading
   master lock. Therefore, we should not have OCaml code going in Val_sexp, because
   that would trigger allocation, and GC stuff, while at the same time, R code might
   be executed, whose C code's underlying assumptions rely on not having garbage
   collection between two statements. So on should ensure that Val_sexp is not called
   in way that multithreads with R code. */


/**  Finalisation function called when OCaml's GC deallocates wrapped R values.
  *
  *  @note This tells R's GC to release this value.
  *  @param valsexp An OCaml value wrapping an R value.
  */
static void r_valsexp_finalisation (value valsexp) {
  R_ReleaseObject(*((SEXP *) Data_custom_val(valsexp)));
}


/**  Static structure used for custom blocks wrapping R values.
  */
static struct custom_operations r_sexp_ops = {
  "org.homelinux.yziquel.OCaml-R",
  r_valsexp_finalisation,
  custom_compare_default,
  custom_hash_default,
  custom_serialize_default,
  custom_deserialize_default
};


/**  Wraps up an R value into an OCaml block.
  *
  *  The R value is preserved from R garbage collection, then
  *  wrapped in an OCaml custom block, with finalisation function
  *  chaining OCaml's GC with R's GC.
  *
  *  @param sexp An R value.
  *  @return An R value wrapped in a OCaml block.
  */
CAMLprim value Val_sexp (SEXP sexp) {
  R_PreserveObject(sexp);  // registers the SEXP as an R GC root.
  value result = caml_alloc_custom(&r_sexp_ops, sizeof(SEXP), 0, 1);
  (*((SEXP *) Data_custom_val(result))) = sexp;
  return result;
}


/**  Macro accessing the underlying SEXP wrapped in an OCaml block.
  *
  *  @param sexp An OCaml block wrapping up an R value.
  *  @return The underlying R value.
  */
#define Sexp_val(sexp) (*((SEXP *) Data_custom_val(sexp)))


/**  Macro wrapping up a R vector SEXP into an OCaml custom block.
  *
  *  @param x An R vector SEXP.
  *  @return An OCaml block wrapping up the R vector SEXP.
  */
#define Val_vecsexp(x) Val_sexp(x)


/**  Macro accessing the underlying vector SEXP in an OCaml block.
  *
  *  @param x An OCaml block wrapping up an R vector value.
  *  @return The underlying vector SEXP.
  */
#define Vecsexp_val(x) ((VECSEXP) Sexp_val(x))
