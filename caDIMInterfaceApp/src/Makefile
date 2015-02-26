TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

USR_INCLUDES += -I$(DIMDIR)/dim
USR_LIBS += dim
dim_DIR += $(DIMDIR)/linux/


#==================================================
# Build an IOC support library

LIBRARY_IOC += caDIMInterfaceSupport

# The following are compiled and added to the Support library
caDIMInterfaceSupport_SRCS += devcaDIMInterfaceSoft.c disassembleString.c ptools.c
caDIMInterfaceSupport_LIBS += $(EPICS_BASE_IOC_LIBS)

#=============================
# build an ioc application

PROD_IOC = caDIMInterface
# caDIMInterface.dbd will be created and installed
DBD += caDIMInterface.dbd

# caDIMInterface.dbd will be made up from these files:
caDIMInterface_DBD += base.dbd
caDIMInterface_DBD += caDIMInterfaceSupport.dbd
#caDIMInterface_DBD += caDIMInterfaceHello.dbd

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
caDIMInterface_SRCS += caDIMInterface_registerRecordDeviceDriver.cpp
caDIMInterface_SRCS_DEFAULT += caDIMInterfaceMain.cpp
caDIMInterface_SRCS_vxWorks += -nil-

# Add locally compiled object code
#caDIMInterface_SRCS += dbSubExample.c

# The following adds support from base/src/vxWorks
caDIMInterface_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

caDIMInterface_LIBS += caDIMInterfaceSupport

# NOTE: To build SNL programs, SNCSEQ must be defined
# in the <top>/configure/RELEASE file

caDIMInterface_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

