# Making HTTP work
- Accept connections
- Read HTTP Requests
- Send HTTP Messages

# Potential Changes
- Change HTTP stuff to allocate & deallocate self
    - Use malloc/free family
    - Make user provide allocator functions (like zig!)
