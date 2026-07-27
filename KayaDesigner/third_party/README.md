# Third-party dependencies

Put Dear ImGui here:

```text
third_party/imgui/
```

This repository intentionally does not vendor SDL3 or Dear ImGui yet. Install SDL3
with your package manager or point CMake to your SDL3 package, then copy Dear
ImGui source files into `third_party/imgui`.

