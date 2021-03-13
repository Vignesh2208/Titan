file(REMOVE_RECURSE
  "syscall_intercept_scoped.o"
  "syscall_intercept_unscoped.o"
  "libsyscall_intercept.pdb"
  "libsyscall_intercept.a"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/syscall_intercept_static.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
