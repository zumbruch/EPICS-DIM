#!../../bin/linux-x86/caDIMInterface

## You may have to change caDIMClient to something else
## everywhere it appears in this file

< envPaths

cd ${TOP}

## Register all support components
dbLoadDatabase("dbd/caDIMInterface.dbd")
caDIMInterface_registerRecordDeviceDriver(pdbbase)

## Load record instances

#dbLoadRecords("db/dbDIMInterface.db","any=, record=ai, INPOUT=INP, user=$(USER), pvName=ServerSvcCmd2,  value=112, Type=@T\=S, serviceName=@S\=Jeder1, serviceFormat=, commandName=@C\=Man, commandFormat=, serverName=@N\=MyIOC-II, dnsAddress=, dnsPort=, defaultPut=, defaultGet=, scanMode=, scanInterval=, scanPeriod=Passive, precision=2")
#dbLoadRecords("db/dbDIMInterface.db","any=, record=ai, INPOUT=INP, user=$(USER), pvName=SServerSvcCmd2,  value=112, Type=@T\=S, serviceName=@S\=Jeder2, serviceFormat=, commandName=@C\=Man2, commandFormat=, serverName=@N\=MyIOC-II, dnsAddress=, dnsPort=, defaultPut=, defaultGet=, scanMode=, scanInterval=, scanPeriod=Passive, precision=2")
#dbLoadRecords("db/dbDIMInterface.db","any=, record=ai, INPOUT=INP, user=$(USER), pvName=ClientSvcCmd,   value=112, Type=@T\=C, serviceName=@S\=Jeder3,  serviceFormat=@Sf\=F:1, commandName=@C\=Man4, commandFormat=@Cf\=F:1, serverName=, dnsAddress=, dnsPort=, defaultPut=, defaultGet=, scanMode=, scanInterval=, scanPeriod=Passive, precision=2")
#dbLoadRecords("db/dbDIMInterface.db","any=, record=ai, INPOUT=INP, user=$(USER), pvName=DClientSvcCmd,  value=112, Type=@T\=C, serviceName=@S\=Jeder3,  serviceFormat=@Sf\=S:1, commandName=@C\=Man5, commandFormat=@Cf\=S:1, serverName=, dnsAddress=, dnsPort=, defaultPut=, defaultGet=, scanMode=, scanInterval=, scanPeriod=Passive, precision=2")

dbLoadRecords("db/dbDIMInterface.db", "any=, record=ai, INPOUT=INP, user=$(USER), pvName=DoubleValClient, value=100, Type=@T\=C, serviceName=@S\=TEST/DOUBLEVAL, serviceFormat=@Sf\=D:1, commandName=,                     commandFormat=,         serverName=, dnsAddress=, dnsPort=, defaultPut=, defaultGet=, scanMode=, scanInterval=, scanPeriod=Passive, precision=2")
dbLoadRecords("db/dbDIMInterface.db", "any=, record=ai, INPOUT=INP, user=$(USER), pvName=IntValClient,    value=0,   Type=@T\=C, serviceName=@S\=TEST/INTVAL,    serviceFormat=@Sf\=I:1, commandName=,                     commandFormat=,         serverName=, dnsAddress=, dnsPort=, defaultPut=, defaultGet=, scanMode=, scanInterval=, scanPeriod=Passive, precision=0")
dbLoadRecords("db/dbDIMInterface.db", "any=, record=ai, INPOUT=INP, user=$(USER), pvName=KillTestServer,  value=0,   Type=@T\=C, serviceName=,                   serviceFormat=,         commandName=@C\=TEST-SERVER/EXIT, commandFormat=@Cf\=L:1, serverName=, dnsAddress=, dnsPort=, defaultPut=, defaultGet=, scanMode=, scanInterval=, scanPeriod=Passive, precision=2")

dbLoadRecords("db/dbDIMInterface.db", "any=, record=ao, INPOUT=OUT, user=$(USER), pvName=ServerAo,  value=10,   Type=@T\=S, serviceName=, serviceFormat=, commandName=, commandFormat=, serverName=, dnsAddress=, dnsPort=, defaultPut=, defaultGet=, scanMode=, scanInterval=, scanPeriod=Passive, precision=2")


## Set this to see messages from mySub
#var mySubDebug 1

cd ${TOP}/iocBoot/${IOC}
iocInit()

## Start any sequence programs
#seq sncExample,"user=brandHost"
