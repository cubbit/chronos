"""
Loads third party depdendencies
"""

load("//third_party/marl:marl.bzl", "marl")

def third_party_dependencies():
    marl()
