# lamac

```sh
usage:
  ./build/lamac tokens <source.lama>
  ./build/lamac ast <source.lama>
  ./build/lamac compile <source.lama> [output]
```

The assembly is saved as `<output>.s`. The runtime is loaded from
`$LAMA_RUNTIME/runtime.a`, or `runtime/runtime.a` if `LAMA_RUNTIME` is unset.
