###############################################################################
#
# 
#
###############################################################################
THIS_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
x86_64_accton_as9737_32db_INCLUDES := -I $(THIS_DIR)inc
x86_64_accton_as9737_32db_INTERNAL_INCLUDES := -I $(THIS_DIR)src
x86_64_accton_as9737_32db_DEPENDMODULE_ENTRIES := init:x86_64_accton_as9737_32db ucli:x86_64_accton_as9737_32db

