# Kernel continuation stack diagnostics

Each task owns a private supervisor stack used by the resumable kernel
continuation path. The stack is allocated in page-order units and currently
has a low-address canary plus a poisoned fill pattern. The canary detects
downward corruption; the fill pattern provides a runtime high-water mark.
Neither is a hardware guard page or a complete safety proof.

## Measuring static usage

Configure a separate build with:

```sh
cmake -S . -B build-stack-usage -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_KERNEL_STACK_USAGE=ON
cmake --build build-stack-usage --target kernel.bin -j$(nproc)
find build-stack-usage/kernel -name '*.su'
```

The option applies `-fstack-usage` only to kernel C sources. NASM sources are
left untouched. Each `.su` record contains the source location, function,
static frame size, and whether GCC could prove the function is static,
dynamic, or bounded. Use the records to inspect deep call chains; they do not
account for recursion, indirect calls, interrupt nesting, or assembly frames.

Runtime watermark output must therefore be treated as complementary evidence,
not as a replacement for static review. Any reduction of the configured stack
budget requires both measurements and stress-test coverage.
