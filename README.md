# lamac

```sh
usage:
  ./build/lamac tokens <source.lama>
  ./build/lamac ast <source.lama>
  ./build/lamac compile <source.lama> [output]
```

The assembly is saved as `<output>.s`. The runtime is loaded from
`$LAMA_RUNTIME/runtime.a`, or `runtime/runtime.a` if `LAMA_RUNTIME` is unset.

Build the compiler and runtime, then run regression tests:

```sh
cmake --build build
make -C runtime
make -C regression
```

To run a single test, use `make -C regression test001`.
`make -C regression clean` removes generated executables, assembly and logs.
