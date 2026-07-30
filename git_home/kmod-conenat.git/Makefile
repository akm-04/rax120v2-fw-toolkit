#
# Makefile for RTL8366SR switch driver
#
# Copyright (C) 2008 Jonathan Chang <jonathan.chang@delta.com.tw>
#

obj-m	:= ipt_CONENAT.o

ipt_CONENAT-objs := ipt_conenat.o

ifeq ($(MAKING_MODULES),1)
#export-objs := ipt_CONENAT.o

-include $(TOPDIR)/Rules.make
endif
