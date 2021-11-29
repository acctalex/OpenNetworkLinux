###############################################################################
#
# 
#
###############################################################################
THIS_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
x86_64_accton_es8632bt4_INCLUDES := -I $(THIS_DIR)inc
x86_64_accton_es8632bt4_INTERNAL_INCLUDES := -I $(THIS_DIR)src
x86_64_accton_es8632bt4_DEPENDMODULE_ENTRIES := init:x86_64_accton_es8632bt4 ucli:x86_64_accton_es8632bt4

