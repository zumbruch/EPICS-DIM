#!../../bin/linux-x86/caDIMInterface

## You may have to change caDIMClient to something else
## everywhere it appears in this file

< envPaths

cd ${TOP}

## Register all support components
dbLoadDatabase("dbd/caDIMInterface.dbd")
caDIMInterface_registerRecordDeviceDriver(pdbbase)

## Load record instances

#dbLoadRecords("db/dbDIMInterface.db","any=, record=ai, INPOUT=INP, user=$(USER), pvName=ServerSvcCmd2, value=112, Type=@T\=S, serviceName=@S\=Jeder1, serviceFormat=, commandName=@C\=Man, commandFormat=, serverName=@N\=MyIOC-II, dnsAddress=, dnsPort=, defaultPut=, defaultGet=, scanMode=, scanInterval=, scanPeriod=Passive, precision=1")
#dbLoadRecords("db/dbDIMInterface.db","any=, record=ai, INPOUT=INP, user=$(USER), pvName=SServerSvcCmd2, value=112, Type=@T\=S, serviceName=@S\=Jeder2, serviceFormat=, commandName=@C\=Man, commandFormat=, serverName=@N\=MyIOC-II, dnsAddress=, dnsPort=, defaultPut=, defaultGet=, scanMode=, scanInterval=, scanPeriod=Passive, precision=1")
#dbLoadRecords("db/dbDIMInterface.db","any=, record=ai, INPOUT=INP, user=$(USER), pvName=ClientSvcCmd, value=112, Type=@T\=C, serviceName=@S\=Jeder2, serviceFormat=, commandName=, commandFormat=, serverName=, dnsAddress=, dnsPort=, defaultPut=, defaultGet=, scanMode=, scanInterval=, scanPeriod=Passive, precision=1")


#example for ao demo
#dbLoadRecords("db/dbDIMInterface.db","any=, record=ao, INPOUT=OUT, user=$(USER), pvName=ServerInteger,         value=112, Type=@T\=S, serviceName=,                                serviceFormat=@Sf\=I:1 , commandName=,        commandFormat=@Cf\=I:1, serverName=@N\=Demo, dnsAddress=, dnsPort=, defaultPut=, defaultGet=, scanMode=,          scanInterval=, scanPeriod=Passive, precision=1")
#dbLoadRecords("db/dbDIMInterface.db","any=, record=ao, INPOUT=OUT, user=$(USER), pvName=ClientServiceIntegerM, value=90,  Type=@T\=C, serviceName=@S\=INTVAL,                      serviceFormat=@Sf\=I:1 , commandName=,        commandFormat=,         serverName=@N\=TEST, dnsAddress=, dnsPort=, defaultPut=, defaultGet=, scanMode=@Ss\=M,    scanInterval=, scanPeriod=Passive, precision=1")
#dbLoadRecords("db/dbDIMInterface.db","any=, record=ao, INPOUT=OUT, user=$(USER), pvName=ClientServiceIntegerT, value=90,  Type=@T\=C, serviceName=@S\=$(USER):ServerInteger_CAGET, serviceFormat=@Sf\=I:1 , commandName=,        commandFormat=,         serverName=@N\=Demo, dnsAddress=, dnsPort=, defaultPut=, defaultGet=, scanMode=@Ss\=T:10, scanInterval=, scanPeriod=Passive, precision=1")


#example for ai demo
dbLoadRecords("db/dbDIMInterface.db","any=, record=ai, INPOUT=INP, user=$(USER), pvName=ServerInteger,         value=112, Type=@T\=S, serviceName=,                                serviceFormat=@Sf\=I:1 , commandName=,        commandFormat=@Cf\=I:1, serverName=@N\=Demo, dnsAddress=, dnsPort=, defaultPut=, defaultGet=, scanMode=,          scanInterval=, scanPeriod=Passive, precision=1")
dbLoadRecords("db/dbDIMInterface.db","any=, record=ai, INPOUT=INP, user=$(USER), pvName=ClientServiceIntegerM, value=90,  Type=@T\=C, serviceName=@S\=INTVAL,                      serviceFormat=@Sf\=I:1 , commandName=,        commandFormat=,         serverName=@N\=TEST, dnsAddress=, dnsPort=, defaultPut=, defaultGet=, scanMode=@Ss\=M,    scanInterval=, scanPeriod=Passive, precision=1")
dbLoadRecords("db/dbDIMInterface.db","any=, record=ai, INPOUT=INP, user=$(USER), pvName=ClientServiceIntegerT, value=90,  Type=@T\=C, serviceName=@S\=$(USER):ServerInteger_CAGET, serviceFormat=@Sf\=I:1 , commandName=,        commandFormat=,         serverName=@N\=Demo, dnsAddress=, dnsPort=, defaultPut=, defaultGet=, scanMode=@Ss\=T:10, scanInterval=, scanPeriod=Passive, precision=1")

## Set this to see messages from mySub
#var mySubDebug 1

cd ${TOP}/iocBoot/${IOC}
iocInit()

## Start any sequence programs
#seq sncExample,"user=brandHost"
