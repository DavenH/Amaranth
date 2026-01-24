
## Runtime Considerations

Avoid allocating heap memory in audio threads. This takes a global lock and can cause stuttering. Instead, preallocate
reasonable amounts of memory in working buffers, and reuse it, taking care not to have multiple threads
writing the same working buffer.

There are utility methods on Buffer for easily taking blocks of working memory:
```cpp
// at initialization time
ScopedAlloc<float> workingMemory(4096);
...
// in some working loop
workingMemory.resetPlacement();
Buffer<float> block1 = workingMemory.place(512); // takes [0 - 511]
Buffer<float> block2 = workingMemory.place(512); // takes [512 - 1023]
Buffer<float> block3 = workingMemory.place(512); // takes [1024 - 1575] etc

// when done with those blocks
workingMemory.resetPlacement();
Buffer<float> block4 = workingMemory.place(512); // takes [0 - 511]
...
```

