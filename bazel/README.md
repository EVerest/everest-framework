# using everst-framework from bazel workspace.

Add the following to your WORKSPACE file:
```
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

EVEREST_FRAMEWORK_REVISION = "4c0488de5eca3a9bc7c9258fc3715c5d6aab8c10"

http_archive(
  name = "com_github_everest_everest-framework",
  url = "https://github.com/EVerest/everest-framework/archive/{}.tar.gz".format(EVEREST_FRAMEWORK_REVISION),
  strip_prefix = "everest-framework-4c0488de5eca3a9bc7c9258fc3715c5d6aab8c10",
)

# This load some definitions need to load dependencies on the next step
load("@com_github_everest_everest-framework//bazel:repos.bzl", "everest_framework_repos")
everest_framework_repos()

# Load all dependencies
load("@com_github_everest_everest-framework//bazel:deps.bzl", "everest_framework_deps")
everest_framework_deps()

```

After that, framework library will be available as `@com_github_everest_everest-framework//:framework` and the manager binary as `@com_github_everest_everest-framework//:manager`
