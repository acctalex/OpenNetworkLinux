###############################################################################
#
# 
#
###############################################################################
THIS_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
x86_64_accton_as9736_64d_INCLUDES := -I $(THIS_DIR)inc
x86_64_accton_as9736_64d_INTERNAL_INCLUDES := -I $(THIS_DIR)src
x86_64_accton_as9736_64d_DEPENDMODULE_ENTRIES := init:x86_64_accton_as9736_64d ucli:x86_64_accton_as9736_64d

