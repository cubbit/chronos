"""
Setup marl
"""

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def marl():
    maybe(
        git_repository,
        "marl",
        remote = "https://github.com/google/marl",
        commit = "49602432d97222eec1e6c8e4f70723c3864c49c1",
        shallow_since = "1629280309 +0100",
        patches = ["@io_cubbit_chronos//third_party/marl:marl.patch"],
        patch_args = ["-p1"],
    )
