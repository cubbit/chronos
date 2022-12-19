load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

def marl():
    git_repository(
        name = "marl",
        remote = "https://github.com/google/marl",
        commit = "49602432d97222eec1e6c8e4f70723c3864c49c1",
        shallow_since = "1629280309 +0100",
        patches = ["//third_party/marl:marl.patch"],
        patch_args = ["-p1"],
    )
