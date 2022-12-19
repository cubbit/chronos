"""
Loads chronos depdendencies
"""

load("//third_party:deps.bzl", "third_party_dependencies")

def chronos_dependencies():
    third_party_dependencies()
