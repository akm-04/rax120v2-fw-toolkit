#
# Makefile for RTL8366SR switch driver
#
# Copyright (C) 2008 Jonathan Chang <jonathan.chang@delta.com.tw>
#

obj-m	:= ip6t_CONE.o

ip6t_CONE-objs := ip6t_cone.o

ifeq ($(MAKING_MODULES),1)
#export-objs := ipt_CONENAT.o

-include $(TOPDIR)/Rules.make
endif
